#include "coverageservice.h"
#include "projectpages.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QtMath>

namespace {
QString requirementScope(const CoverageScope &scope) {
  QStringList clauses;
  if (!scope.includeObsolete)
    clauses << "NOT EXISTS(SELECT 1 FROM REQ_STATUS S WHERE S.ID=R.STATUS "
               "AND (UPPER(S.STATUS)='OBSOLETE' OR UPPER(S.SHORTCUT)='O'))";
  if (!scope.statusIds.isEmpty()) {
    QStringList ids;
    for (int id : scope.statusIds)
      ids << QString::number(id);
    clauses << QString("R.STATUS IN(%1)").arg(ids.join(','));
  }
  if (scope.typeId >= 0)
    clauses << QString("R.TYPE=%1").arg(scope.typeId);
  if (scope.documentId >= 0)
    clauses << QString("(R.DOC_ID=%1 OR EXISTS(SELECT 1 FROM DOCUMENT_NODE DN "
                       "WHERE DN.REQ_ID=R.ID AND DN.DOC_ID=%1))")
                   .arg(scope.documentId);
  if (scope.ptId >= 0)
    clauses << QString("EXISTS(SELECT 1 FROM REQUIREMENT_PT RP WHERE "
                       "RP.REQ_ID=R.ID AND RP.PT_ID=%1)")
                   .arg(scope.ptId);
  if (scope.configurationId >= 0)
    clauses << QString("EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY RA "
                       "WHERE RA.REQ_ID=R.ID AND RA.CONFIG_ID=%1)")
                   .arg(scope.configurationId);
  if (!scope.search.trimmed().isEmpty()) {
    QString text = scope.search.trimmed();
    text.replace(QChar(39), "''");
    clauses << QString("(R.CODE LIKE '%%1%' OR R.TITLE LIKE '%%1%' OR "
                       "COALESCE(R.DESCRIPTION,'') LIKE '%%1%')")
                   .arg(text);
  }
  return clauses.isEmpty() ? QStringLiteral("1=1") : clauses.join(" AND ");
}

int scalar(const QSqlDatabase &db, const QString &sql, QString *error) {
  QSqlQuery query(db);
  if (!query.exec(sql) || !query.next()) {
    if (error && error->isEmpty())
      *error = query.lastError().text();
    return 0;
  }
  return query.value(0).toInt();
}

QList<CoverageSlice> slices(const QSqlDatabase &db, const QString &sql,
                            QString *error) {
  QList<CoverageSlice> result;
  QSqlQuery query(db);
  if (!query.exec(sql)) {
    if (error && error->isEmpty()) *error = query.lastError().text();
    return result;
  }
  while (query.next())
    result << CoverageSlice{query.value(0).toInt(), query.value(1).toString(),
                            query.value(2).toString(), query.value(3).toInt()};
  return result;
}
}

int CoverageMetric::percent() const {
  return total > 0 ? qRound(100.0 * covered / total) : 0;
}
int ConfigurationCoverage::percent() const {
  return total > 0 ? qRound(100.0 * applicable / total) : 0;
}

CoverageService::CoverageService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

void CoverageService::setConnectionName(const QString &connectionName) {
  m_connectionName = connectionName;
}

bool CoverageService::isAvailable() const {
  if (m_connectionName.isEmpty() || !QSqlDatabase::contains(m_connectionName))
    return false;
  const auto db = QSqlDatabase::database(m_connectionName, false);
  return db.isValid() && db.isOpen();
}

QList<CoverageMetric> CoverageService::metrics(const CoverageScope &scope,
                                               QString *error) const {
  if (error)
    error->clear();
  if (!isAvailable()) {
    if (error)
      *error = QStringLiteral("Aucun projet ouvert.");
    return {};
  }
  const auto db = QSqlDatabase::database(m_connectionName, false);
  const QString where = requirementScope(scope);
  const int requirements =
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where, error);
  const int traceable = scalar(
      db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
              " AND COALESCE(R.IS_TRACE_ROOT,0)=0",
      error);
  const int interfaces = scalar(
      db, "SELECT COUNT(*) FROM INTERFACE I WHERE COALESCE(I.ARCHIVED,0)=0",
      error);
  const int openChanges = scalar(
      db, "SELECT COUNT(*) FROM CHANGE_ITEM C JOIN CHANGE_STATUS S ON "
          "S.ID=C.STATUS_ID WHERE COALESCE(C.ARCHIVED,0)=0 AND S.IS_FINAL=0",
      error);
  const int allChanges = scalar(
      db, "SELECT COUNT(*) FROM CHANGE_ITEM C WHERE COALESCE(C.ARCHIVED,0)=0",
      error);

  QList<CoverageMetric> result;
  result << CoverageMetric{"requirements", "Exigences", requirements,
                           requirements, 1, "all", "all"};
  result << CoverageMetric{
      "allocation", "Allocation Product Tree",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
                     " AND EXISTS(SELECT 1 FROM REQUIREMENT_PT X WHERE "
                     "X.REQ_ID=R.ID)", error),
      requirements, 1, "allocated", "unallocated"};
  result << CoverageMetric{
      "traceability", "Traçabilité amont",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
                     " AND COALESCE(R.IS_TRACE_ROOT,0)=0 AND EXISTS(SELECT 1 "
                     "FROM REQUIREMENT_RELATION X WHERE X.TARGET_REQ_ID=R.ID "
                     "AND X.TYPE_ID IN(1,2))", error),
      traceable, 1, "traced", "untraced"};
  result << CoverageMetric{
      "documentation", "Couverture documentaire",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
                     " AND EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE "
                     "N.REQ_ID=R.ID)", error),
      requirements, 1, "documented", "undocumented"};
  result << CoverageMetric{
      "verification", "Planification vérification",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
                     " AND EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V "
                     "WHERE V.REQ_ID=R.ID AND V.METHOD_ID IS NOT NULL AND "
                     "V.VERIFICATION_LEVEL_PT_ID IS NOT NULL)", error),
      requirements, 1, "verified", "unverified"};
  result << CoverageMetric{
      "applicability", "Applicabilité définie",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
                     " AND EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A "
                     "WHERE A.REQ_ID=R.ID)", error),
      requirements, 1, "applicable", "no-applicability"};
  result << CoverageMetric{
      "interfaces", "Interfaces couvertes ICD",
      scalar(db, "SELECT COUNT(*) FROM INTERFACE I WHERE "
                 "COALESCE(I.ARCHIVED,0)=0 AND EXISTS(SELECT 1 FROM "
                 "INTERFACE_DOCUMENT X WHERE X.INTERFACE_ID=I.ID)", error),
      interfaces, InterfacesPage, "covered", "uncovered"};
  const int documents = scalar(db, "SELECT COUNT(*) FROM DOCUMENT", error);
  result << CoverageMetric{"documents", "Documents", documents, documents, DocumentsPage,
                           "all", "all"};
  const int publications = scalar(
      db, "SELECT COUNT(*) FROM DOCUMENT_EXPORT WHERE "
          "EXPORT_KIND='PUBLICATION'", error);
  result << CoverageMetric{
      "publications", "Publications (30 jours)",
      scalar(db, "SELECT COUNT(*) FROM DOCUMENT_EXPORT WHERE "
                 "EXPORT_KIND='PUBLICATION' AND "
                 "EXPORTED_AT>=DATETIME('now','-30 day')", error),
      publications, DocumentsPage, "publications", "publications"};
  result << CoverageMetric{"open-changes", "Changements ouverts", openChanges,
                           allChanges, ChangesPage, "open", "closed"};
  result << CoverageMetric{
      "decided-changes", "Changements décidés",
      scalar(db, "SELECT COUNT(*) FROM CHANGE_ITEM C JOIN CHANGE_STATUS S ON "
                 "S.ID=C.STATUS_ID WHERE COALESCE(C.ARCHIVED,0)=0 AND "
                 "S.IS_FINAL=1 AND TRIM(COALESCE(C.DECISION,''))<>''", error),
      allChanges, ChangesPage, "decided", "incomplete"};
  return result;
}

CoverageSnapshot CoverageService::snapshot(const CoverageScope &scope,
                                           QString *error) const {
  CoverageSnapshot out;
  out.metrics = metrics(scope, error);
  if (!isAvailable() || (error && !error->isEmpty())) return out;
  const auto db = QSqlDatabase::database(m_connectionName, false);
  const QString where = requirementScope(scope);
  out.requirements = scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where, error);
  out.requirementStatuses = slices(
      db, "SELECT S.ID,S.SHORTCUT,S.STATUS,COUNT(R.ID) FROM REQ_STATUS S "
          "LEFT JOIN REQUIREMENT R ON R.STATUS=S.ID AND " + where +
          " GROUP BY S.ID,S.SHORTCUT,S.STATUS ORDER BY S.ID", error);
  out.changeTypes = slices(db,
      "SELECT T.ID,T.CODE,T.LABEL,COUNT(C.ID) FROM CHANGE_TYPE T LEFT JOIN "
      "CHANGE_ITEM C ON C.TYPE_ID=T.ID AND COALESCE(C.ARCHIVED,0)=0 WHERE "
      "T.ACTIVE=1 GROUP BY T.ID,T.CODE,T.LABEL ORDER BY T.ID", error);
  out.changeStatuses = slices(db,
      "SELECT S.ID,S.CODE,S.LABEL,COUNT(C.ID) FROM CHANGE_STATUS S LEFT JOIN "
      "CHANGE_ITEM C ON C.STATUS_ID=S.ID AND COALESCE(C.ARCHIVED,0)=0 GROUP BY "
      "S.ID,S.CODE,S.LABEL ORDER BY S.POSITION,S.ID", error);
  out.documentTypes = slices(db,
      "SELECT T.ID,T.TYPE,T.TYPE,COUNT(D.ID) FROM DOC_TYPE T LEFT JOIN DOCUMENT "
      "D ON D.TYPE=T.ID GROUP BY T.ID,T.TYPE ORDER BY T.ID", error);
  out.documents = scalar(db, "SELECT COUNT(*) FROM DOCUMENT", error);
  out.changes = scalar(db, "SELECT COUNT(*) FROM CHANGE_ITEM WHERE COALESCE(ARCHIVED,0)=0", error);
  out.openChanges = scalar(db, "SELECT COUNT(*) FROM CHANGE_ITEM C JOIN CHANGE_STATUS S ON S.ID=C.STATUS_ID WHERE COALESCE(C.ARCHIVED,0)=0 AND S.IS_FINAL=0", error);
  out.activeConfigurations = scalar(db, "SELECT COUNT(*) FROM CONFIGURATION WHERE ACTIVE=1", error);
  out.recentPublications = scalar(db, "SELECT COUNT(*) FROM DOCUMENT_EXPORT WHERE EXPORT_KIND='PUBLICATION' AND EXPORTED_AT>=DATETIME('now','-30 day')", error);
  out.neverPublishedDocuments = scalar(db, "SELECT COUNT(*) FROM DOCUMENT D WHERE NOT EXISTS(SELECT 1 FROM DOCUMENT_EXPORT E WHERE E.DOC_ID=D.ID AND E.EXPORT_KIND='PUBLICATION')", error);
  QSqlQuery configs(db);
  if (!configs.exec("SELECT ID,CODE,LABEL,ACTIVE FROM CONFIGURATION ORDER BY POSITION,CODE")) {
    if (error && error->isEmpty()) *error = configs.lastError().text();
    return out;
  }
  while (configs.next()) {
    ConfigurationCoverage c;
    c.id=configs.value(0).toInt(); c.code=configs.value(1).toString();
    c.label=configs.value(2).toString(); c.active=configs.value(3).toBool();
    c.total=out.requirements;
    c.applicable=scalar(db, "SELECT COUNT(DISTINCT R.ID) FROM REQUIREMENT R WHERE " + where +
        QString(" AND EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A WHERE A.REQ_ID=R.ID AND A.CONFIG_ID=%1 AND A.APPLICABLE=1)").arg(c.id), error);
    out.configurations << c;
  }
  return out;
}
