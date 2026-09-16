#include "historyservice.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

HistoryService::HistoryService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

static QSqlDatabase historyDb(const QString &name) {
  return QSqlDatabase::contains(name)
             ? QSqlDatabase::database(name, false)
             : QSqlDatabase();
}

QList<HistoryRecord> HistoryService::find(const QString &text,
                                           const QString &objectType,
                                           const QString &eventType,
                                           int objectId) const {
  QList<HistoryRecord> result;
  QSqlQuery q(historyDb(m_connectionName));
  QString sql = "SELECT ID,EVENT_TIME,COALESCE(AUTHOR,''),EVENT_TYPE,"
                "OBJECT_TYPE,COALESCE(OBJECT_ID,-1),COALESCE(BEFORE_JSON,''),"
                "COALESCE(AFTER_JSON,''),COALESCE(COMMENT,''),"
                "COALESCE(EVENT_GROUP,'') FROM EVENT_LOG WHERE 1=1";
  QVariantList values;
  if (!objectType.isEmpty()) { sql += " AND OBJECT_TYPE=?"; values << objectType; }
  if (!eventType.isEmpty()) { sql += " AND EVENT_TYPE=?"; values << eventType; }
  if (objectId >= 0) { sql += " AND OBJECT_ID=?"; values << objectId; }
  if (!text.trimmed().isEmpty()) {
    sql += " AND (AUTHOR LIKE ? OR OBJECT_TYPE LIKE ? OR EVENT_TYPE LIKE ? OR "
           "BEFORE_JSON LIKE ? OR AFTER_JSON LIKE ? OR COMMENT LIKE ?)";
    const QString pattern = "%" + text.trimmed() + "%";
    for (int i = 0; i < 6; ++i) values << pattern;
  }
  sql += " ORDER BY ID DESC LIMIT 5000";
  q.prepare(sql);
  for (const QVariant &value : values) q.addBindValue(value);
  if (!q.exec()) return result;
  while (q.next()) {
    HistoryRecord r;
    r.id=q.value(0).toInt(); r.time=q.value(1).toString();
    r.author=q.value(2).toString(); r.eventType=q.value(3).toString();
    r.objectType=q.value(4).toString(); r.objectId=q.value(5).toInt();
    r.beforeJson=q.value(6).toString(); r.afterJson=q.value(7).toString();
    r.comment=q.value(8).toString(); r.groupId=q.value(9).toString();
    result << r;
  }
  return result;
}

static QStringList distinctValues(const QString &connection,
                                  const QString &column) {
  QStringList values;
  QSqlQuery q("SELECT DISTINCT " + column + " FROM EVENT_LOG ORDER BY " + column,
              historyDb(connection));
  while (q.next()) values << q.value(0).toString();
  return values;
}

QStringList HistoryService::objectTypes() const {
  return distinctValues(m_connectionName, "OBJECT_TYPE");
}
QStringList HistoryService::eventTypes() const {
  return distinctValues(m_connectionName, "EVENT_TYPE");
}

ProjectDiagnostic HistoryService::diagnose() const {
  ProjectDiagnostic d;
  const QSqlDatabase db = historyDb(m_connectionName);
  if (!db.isValid() || !db.isOpen()) { d.errors << "Aucun projet ouvert."; return d; }
  d.fileSize = QFileInfo(db.databaseName()).size();
  QSqlQuery version("PRAGMA user_version", db);
  if (version.next()) d.schemaVersion = version.value(0).toInt();
  QSqlQuery quick("PRAGMA quick_check", db);
  while (quick.next()) if (quick.value(0).toString() != "ok") d.errors << quick.value(0).toString();
  QSqlQuery fk("PRAGMA foreign_key_check", db);
  while (fk.next()) d.errors << QString("Clé étrangère invalide : %1 ligne %2 vers %3")
      .arg(fk.value(0).toString(), fk.value(1).toString(), fk.value(2).toString());
  QSqlQuery integrity("PRAGMA integrity_check", db);
  while (integrity.next()) if (integrity.value(0).toString() != "ok") d.errors << integrity.value(0).toString();
  d.ok = d.errors.isEmpty();
  return d;
}

QString ProjectDiagnostic::summary() const {
  QString text = QString("Schéma : v%1\nTaille : %2 octets\nÉtat : %3")
                     .arg(schemaVersion).arg(fileSize).arg(ok ? "intègre" : "anomalies détectées");
  if (!errors.isEmpty()) text += "\n\nErreurs :\n- " + errors.join("\n- ");
  if (!warnings.isEmpty()) text += "\n\nAvertissements :\n- " + warnings.join("\n- ");
  return text;
}

bool HistoryService::setAuthor(const QString &author, QString *error) const {
  QSqlQuery q(historyDb(m_connectionName));
  q.prepare("UPDATE EVENT_CONTEXT SET AUTHOR=? WHERE ID=1");
  q.addBindValue(author.trimmed());
  if (q.exec()) return true;
  if (error) *error = q.lastError().text();
  return false;
}
