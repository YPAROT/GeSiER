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
    clauses << QString("EXISTS(SELECT 1 FROM DOCUMENT_NODE DN WHERE "
                       "DN.REQ_ID=R.ID AND DN.DOC_ID=%1)")
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
int DocumentRequirementCoverage::percent() const {
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
  const QString applicableWhere =
      where + " AND COALESCE(R.NOT_APPLICABLE,0)=0";
  const int applicableRequirements = scalar(
      db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + applicableWhere, error);
  const int traceable = scalar(
      db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + applicableWhere +
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
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + applicableWhere +
                     " AND EXISTS(SELECT 1 FROM REQUIREMENT_PT X WHERE "
                     "X.REQ_ID=R.ID)", error),
      applicableRequirements, 1, "allocated", "unallocated"};
  result << CoverageMetric{
      "traceability", "Traçabilité amont",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + applicableWhere +
                     " AND COALESCE(R.IS_TRACE_ROOT,0)=0 AND EXISTS(SELECT 1 "
                     "FROM REQUIREMENT_RELATION X WHERE X.TARGET_REQ_ID=R.ID "
                     "AND X.TYPE_ID IN(1,2))", error),
      traceable, 1, "traced", "untraced"};
  result << CoverageMetric{
      "documentation", "Couverture documentaire",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + applicableWhere +
                     " AND EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE "
                     "N.REQ_ID=R.ID)", error),
      applicableRequirements, 1, "documented", "undocumented"};
  result << CoverageMetric{
      "verification", "Planification vérification",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + applicableWhere +
                     " AND EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V "
                     "JOIN PT P ON P.ID=V.VERIFICATION_LEVEL_PT_ID "
                     "WHERE V.REQ_ID=R.ID AND V.METHOD_ID IS NOT NULL AND "
                     "COALESCE(P.ARCHIVED,0)=0)", error),
      applicableRequirements, 1, "verified", "unverified"};
  result << CoverageMetric{
      "applicability", "Applicabilité définie",
      scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + applicableWhere +
                     " AND EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A "
                     "WHERE A.REQ_ID=R.ID)", error),
      applicableRequirements, 1, "applicable", "no-applicability"};
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
  out.applicableRequirements = scalar(
      db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
              " AND COALESCE(R.NOT_APPLICABLE,0)=0",
      error);
  const QString complete =
      "EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V JOIN PT P ON "
      "P.ID=V.VERIFICATION_LEVEL_PT_ID WHERE V.REQ_ID=R.ID AND "
      "V.METHOD_ID IS NOT NULL AND COALESCE(P.ARCHIVED,0)=0)";
  const QString draft =
      "EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V WHERE V.REQ_ID=R.ID "
      "AND V.METHOD_ID IS NOT NULL AND V.VERIFICATION_LEVEL_PT_ID IS NULL)";
  auto stateCount = [&](const QString &condition) {
    return scalar(db, "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + where +
                          " AND " + condition,
                  error);
  };
  out.verificationPlanning.total = out.applicableRequirements;
  const QString projectApplicable = "COALESCE(R.NOT_APPLICABLE,0)=0 AND ";
  out.verificationPlanning.covered =
      stateCount(projectApplicable + complete + " AND NOT " + draft);
  out.verificationPlanning.coveredDraft =
      stateCount(projectApplicable + complete + " AND " + draft);
  out.verificationPlanning.draft =
      stateCount(projectApplicable + "NOT " + complete + " AND " + draft);
  out.verificationPlanning.missing = out.applicableRequirements -
      out.verificationPlanning.covered - out.verificationPlanning.coveredDraft -
      out.verificationPlanning.draft;
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
  {
    const QString applicability = scope.configurationId >= 0
        ? QString("A.CONFIG_ID=%1").arg(scope.configurationId)
        : QStringLiteral("EXISTS(SELECT 1 FROM CONFIGURATION C WHERE "
                         "C.ID=A.CONFIG_ID AND C.ACTIVE=1)");
    QString sql =
        "SELECT D.ID,COALESCE((SELECT X.REFERENCE FROM DOCUMENT_REFERENCE X "
        "WHERE X.DOC_ID=D.ID AND X.IS_PRIMARY=1),'—'),D.TITLE,"
        "COALESCE(T.TYPE,'—'),COALESCE(P.NAME,'—'),"
        "COUNT(DISTINCT CASE WHEN COALESCE(R.NOT_APPLICABLE,0)=0 THEN R.ID END),"
        "COUNT(DISTINCT CASE WHEN EXISTS(SELECT 1 FROM "
        "REQUIREMENT_APPLICABILITY A WHERE A.REQ_ID=R.ID AND A.APPLICABLE=1 "
        "AND " + applicability + ") THEN R.ID END),"
        "COALESCE((SELECT E.VERSION FROM DOCUMENT_EXPORT E WHERE E.DOC_ID=D.ID "
        "AND E.EXPORT_KIND='PUBLICATION' ORDER BY E.EXPORTED_AT DESC,E.ID DESC "
        "LIMIT 1),''),"
        "COALESCE((SELECT E.EXPORTED_AT FROM DOCUMENT_EXPORT E WHERE E.DOC_ID=D.ID "
        "AND E.EXPORT_KIND='PUBLICATION' ORDER BY E.EXPORTED_AT DESC,E.ID DESC "
        "LIMIT 1),'') "
        "FROM DOCUMENT D LEFT JOIN DOC_TYPE T ON T.ID=D.TYPE "
        "LEFT JOIN PT P ON P.ID=D.PT_ID "
        "LEFT JOIN DOCUMENT_NODE DN ON DN.DOC_ID=D.ID AND "
        "DN.NODE_TYPE='REQUIREMENT' LEFT JOIN REQUIREMENT R ON R.ID=DN.REQ_ID "
        "AND (" + where + ") ";
    if (scope.documentId >= 0)
      sql += QString("WHERE D.ID=%1 ").arg(scope.documentId);
    sql += "GROUP BY D.ID,D.TITLE,T.TYPE,P.NAME "
           "ORDER BY COUNT(DISTINCT R.ID) DESC,D.TITLE";
    QSqlQuery documents(db);
    if (!documents.exec(sql)) {
      if (error && error->isEmpty()) *error = documents.lastError().text();
      return out;
    }
    while (documents.next()) {
      DocumentRequirementCoverage row;
      row.id = documents.value(0).toInt();
      row.reference = documents.value(1).toString();
      row.title = documents.value(2).toString();
      row.type = documents.value(3).toString();
      row.productTree = documents.value(4).toString();
      row.total = documents.value(5).toInt();
      row.applicable = documents.value(6).toInt();
      row.publicationVersion = documents.value(7).toString();
      row.publicationDate = documents.value(8).toString();
      out.documentRequirements << row;
    }
  }
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
    c.total=out.applicableRequirements;
    c.applicable=scalar(db, "SELECT COUNT(DISTINCT R.ID) FROM REQUIREMENT R WHERE " + where +
        " AND COALESCE(R.NOT_APPLICABLE,0)=0" +
        QString(" AND EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A WHERE A.REQ_ID=R.ID AND A.CONFIG_ID=%1 AND A.APPLICABLE=1)").arg(c.id), error);
    out.configurations << c;
  }
  return out;
}
