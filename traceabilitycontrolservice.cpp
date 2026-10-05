#include "traceabilitycontrolservice.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
struct Relation {
  int source = -1;
  int target = -1;
  QString code;
  QString label;
};

bool setError(QString *error, const QSqlQuery &query) {
  if (error)
    *error = query.lastError().text();
  return false;
}
}

TraceabilityControlService::TraceabilityControlService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

void TraceabilityControlService::setConnectionName(const QString &connectionName) {
  m_connectionName = connectionName;
}

int TraceabilityControlService::activeRootId(QString *error) const {
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  if (!query.exec("SELECT ID FROM PT WHERE PARENT IS NULL AND ARCHIVED=0 "
                  "ORDER BY ID LIMIT 1")) {
    setError(error, query);
    return -1;
  }
  return query.next() ? query.value(0).toInt() : -1;
}

int TraceabilityControlService::activeConfigurationCount(QString *error) const {
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  if (!query.exec("SELECT COUNT(*) FROM CONFIGURATION WHERE ACTIVE=1")) {
    setError(error, query);
    return 0;
  }
  return query.next() ? query.value(0).toInt() : 0;
}

QList<TraceabilityControlRow>
TraceabilityControlService::rows(const TraceabilityFilter &filter,
                                 QString *error) const {
  if (error)
    error->clear();
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QList<TraceabilityControlRow> all;

  QSqlQuery requirements(db);
  if (!requirements.exec(
          "SELECT R.ID,R.CODE,R.TITLE,R.STATUS,COALESCE(S.STATUS,''),"
          "CASE WHEN UPPER(COALESCE(S.STATUS,''))='OBSOLETE' OR "
          "UPPER(COALESCE(S.SHORTCUT,''))='O' THEN 1 ELSE 0 END,"
          "COALESCE((SELECT PT_ID FROM REQUIREMENT_PT RP WHERE RP.REQ_ID=R.ID "
          "AND RP.IS_PRIMARY=1 LIMIT 1),-1),"
          "COALESCE((SELECT P.NAME FROM REQUIREMENT_PT RP JOIN PT P ON "
          "P.ID=RP.PT_ID WHERE RP.REQ_ID=R.ID AND RP.IS_PRIMARY=1 LIMIT 1),'') "
          "FROM REQUIREMENT R LEFT JOIN REQ_STATUS S ON S.ID=R.STATUS "
          "WHERE COALESCE(R.IS_TRACE_ROOT,0)=1 ORDER BY R.CODE")) {
    setError(error, requirements);
    return all;
  }
  while (requirements.next()) {
    TraceabilityControlRow row;
    row.requirementId = requirements.value(0).toInt();
    row.code = requirements.value(1).toString();
    row.title = requirements.value(2).toString();
    row.statusId = requirements.value(3).toInt();
    row.status = requirements.value(4).toString();
    row.obsolete = requirements.value(5).toBool();
    row.primaryPtId = requirements.value(6).toInt();
    row.primaryPt = requirements.value(7).toString();
    all << row;
  }

  const int rootId = activeRootId(error);
  if (error && !error->isEmpty())
    return {};

  QHash<int, int> ptParent;
  QSqlQuery pts(db);
  if (!pts.exec("SELECT ID,PARENT FROM PT")) {
    setError(error, pts);
    return {};
  }
  while (pts.next())
    ptParent.insert(pts.value(0).toInt(),
                    pts.value(1).isNull() ? -1 : pts.value(1).toInt());
  auto isStrictDescendant = [&ptParent](int candidate, int ancestor) {
    if (candidate < 0 || ancestor < 0 || candidate == ancestor)
      return false;
    QSet<int> visited;
    int current = candidate;
    while (ptParent.contains(current) && !visited.contains(current)) {
      visited.insert(current);
      current = ptParent.value(current);
      if (current == ancestor)
        return true;
      if (current < 0)
        break;
    }
    return false;
  };

  QList<int> activeConfigurations;
  QSqlQuery configurations(db);
  if (!configurations.exec(
          "SELECT ID FROM CONFIGURATION WHERE ACTIVE=1 ORDER BY POSITION,CODE")) {
    setError(error, configurations);
    return {};
  }
  while (configurations.next())
    activeConfigurations << configurations.value(0).toInt();

  // At the project root, a root-specific value has priority over the simple
  // value, exactly as in ApplicabilityService::matrix.
  QHash<QString, bool> simpleApplicability;
  QHash<QString, bool> rootApplicability;
  QSqlQuery applicability(db);
  applicability.prepare(
      "SELECT REQ_ID,CONFIG_ID,PT_ID,APPLICABLE FROM "
      "REQUIREMENT_APPLICABILITY WHERE PT_ID IS NULL OR PT_ID=?");
  applicability.addBindValue(rootId);
  if (!applicability.exec()) {
    setError(error, applicability);
    return {};
  }
  while (applicability.next()) {
    const QString key = QString::number(applicability.value(0).toInt()) + '|' +
                        QString::number(applicability.value(1).toInt());
    if (applicability.value(2).isNull())
      simpleApplicability.insert(key, applicability.value(3).toBool());
    else
      rootApplicability.insert(key, applicability.value(3).toBool());
  }

  QHash<int, int> primaryPt;
  QSqlQuery allocations(db);
  if (!allocations.exec("SELECT REQ_ID,PT_ID FROM REQUIREMENT_PT WHERE "
                        "IS_PRIMARY=1")) {
    setError(error, allocations);
    return {};
  }
  while (allocations.next())
    primaryPt.insert(allocations.value(0).toInt(), allocations.value(1).toInt());

  QList<Relation> relations;
  QSqlQuery relationQuery(db);
  if (!relationQuery.exec(
          "SELECT X.SOURCE_REQ_ID,X.TARGET_REQ_ID,T.CODE,T.LABEL FROM "
          "REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON "
          "T.ID=X.TYPE_ID")) {
    setError(error, relationQuery);
    return {};
  }
  while (relationQuery.next())
    relations << Relation{relationQuery.value(0).toInt(),
                          relationQuery.value(1).toInt(),
                          relationQuery.value(2).toString(),
                          relationQuery.value(3).toString()};

  for (TraceabilityControlRow &row : all) {
    row.rootConform = rootId >= 0 && row.primaryPtId == rootId;
    for (int configurationId : activeConfigurations) {
      const QString key = QString::number(row.requirementId) + '|' +
                          QString::number(configurationId);
      const bool value = rootApplicability.contains(key)
                             ? rootApplicability.value(key)
                             : simpleApplicability.value(key, false);
      if (value) {
        row.applicable = true;
        break;
      }
    }

    QSet<QString> types;
    for (const Relation &relation : relations) {
      int other = -1;
      if ((relation.code == "DECOMPOSE" ||
           relation.code == "DERIVES_FROM") &&
          relation.source == row.requirementId)
        other = relation.target;
      else if (relation.code == "DEPENDS_ON" &&
               relation.target == row.requirementId)
        other = relation.source;
      if (other >= 0 &&
          isStrictDescendant(primaryPt.value(other, -1), row.primaryPtId)) {
        ++row.qualifyingRelationCount;
        types.insert(relation.label);
      }
    }
    row.relationTypes = types.values();
    row.relationTypes.sort(Qt::CaseInsensitive);
    row.takenIntoAccount = row.qualifyingRelationCount > 0;

    QStringList diagnostics;
    if (row.obsolete) {
      diagnostics << "Exigence obsolète — contrôles désactivés";
    } else {
      if (rootId < 0)
        diagnostics << "Aucune racine PT active";
      else if (!row.rootConform)
        diagnostics << "Le PT principal n'est pas la racine active";
      if (!row.applicable)
        diagnostics << "Non applicable au projet";
      else if (!row.takenIntoAccount)
        diagnostics << "Aucune relation qualifiante vers un PT descendant";
      if (diagnostics.isEmpty())
        diagnostics << "Conforme";
    }
    row.diagnostic = diagnostics.join(" ; ");
  }

  QList<TraceabilityControlRow> filtered;
  const QString needle = filter.text.trimmed();
  for (const TraceabilityControlRow &row : all) {
    if (!filter.includeObsolete && row.obsolete)
      continue;
    if (!needle.isEmpty() && !row.code.contains(needle, Qt::CaseInsensitive) &&
        !row.title.contains(needle, Qt::CaseInsensitive))
      continue;
    if (filter.statusId >= 0 && row.statusId != filter.statusId)
      continue;
    if (filter.applicable >= 0 && row.applicable != (filter.applicable == 1))
      continue;
    if (filter.takenIntoAccount >= 0 &&
        row.takenIntoAccount != (filter.takenIntoAccount == 1))
      continue;
    if (filter.rootConform >= 0 &&
        row.rootConform != (filter.rootConform == 1))
      continue;
    filtered << row;
  }
  return filtered;
}
