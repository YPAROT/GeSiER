#include "referencedataservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QSqlDatabase database(const QString &name) {
  return QSqlDatabase::database(name, false);
}
ReferenceDataResult unavailable(const QSqlDatabase &db) {
  return ReferenceDataResult::failure(
      db.isValid() ? QString("La base du projet n'est pas ouverte.")
                   : QString("Connexion au projet introuvable."));
}
}

ReferenceDataService::ReferenceDataService(const QString &connectionName)
    : m_connectionName(connectionName) {}

void ReferenceDataService::setConnectionName(const QString &connectionName) {
  m_connectionName = connectionName;
}

QList<ReferenceDataEntry>
ReferenceDataService::verificationMethods(QString *error) const {
  QList<ReferenceDataEntry> result;
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen()) {
    if (error) *error = unavailable(db).message;
    return result;
  }
  QSqlQuery query(db);
  if (!query.exec("SELECT ID,METHOD FROM REQ_METHOD ORDER BY METHOD COLLATE NOCASE,ID")) {
    if (error) *error = query.lastError().text();
    return result;
  }
  while (query.next()) result << ReferenceDataEntry{query.value(0).toInt(), {}, query.value(1).toString()};
  return result;
}

QList<ReferenceDataEntry>
ReferenceDataService::requirementTypes(QString *error) const {
  QList<ReferenceDataEntry> result;
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen()) {
    if (error) *error = unavailable(db).message;
    return result;
  }
  QSqlQuery query(db);
  if (!query.exec("SELECT ID,COALESCE(CODE,''),TYPE FROM REQ_TYPE ORDER BY TYPE COLLATE NOCASE,ID")) {
    if (error) *error = query.lastError().text();
    return result;
  }
  while (query.next()) result << ReferenceDataEntry{query.value(0).toInt(), query.value(1).toString(), query.value(2).toString()};
  return result;
}

ReferenceDataResult ReferenceDataService::addVerificationMethod(const QString &label) const {
  return saveMethod(-1, label);
}
ReferenceDataResult ReferenceDataService::updateVerificationMethod(int id, const QString &label) const {
  return saveMethod(id, label);
}
ReferenceDataResult ReferenceDataService::saveMethod(int id, const QString &value) const {
  const QString label = value.trimmed();
  if (label.isEmpty()) return ReferenceDataResult::failure("Le libellé de la méthode est obligatoire.");
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen()) return unavailable(db);
  QSqlQuery duplicate(db);
  if (!duplicate.exec("SELECT ID,METHOD FROM REQ_METHOD"))
    return ReferenceDataResult::failure(duplicate.lastError().text());
  while (duplicate.next())
    if (duplicate.value(0).toInt() != id &&
        duplicate.value(1).toString().trimmed().compare(label, Qt::CaseInsensitive) == 0)
      return ReferenceDataResult::failure("Une méthode de vérification porte déjà ce libellé.");
  QSqlQuery query(db);
  if (id < 0) query.prepare("INSERT INTO REQ_METHOD(METHOD) VALUES(?)");
  else query.prepare("UPDATE REQ_METHOD SET METHOD=? WHERE ID=?");
  query.addBindValue(label); if (id >= 0) query.addBindValue(id);
  if (!query.exec()) return ReferenceDataResult::failure(query.lastError().text());
  return ReferenceDataResult::ok(id < 0 ? query.lastInsertId().toInt() : id);
}

ReferenceDataResult ReferenceDataService::addRequirementType(const QString &code, const QString &label) const {
  return saveType(-1, code, label);
}
ReferenceDataResult ReferenceDataService::updateRequirementType(int id, const QString &code, const QString &label) const {
  return saveType(id, code, label);
}
ReferenceDataResult ReferenceDataService::saveType(int id, const QString &rawCode, const QString &rawLabel) const {
  const QString code = rawCode.trimmed(), label = rawLabel.trimmed();
  if (label.isEmpty()) return ReferenceDataResult::failure("Le libellé du type d'exigence est obligatoire.");
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen()) return unavailable(db);
  QSqlQuery duplicate(db);
  if (!duplicate.exec("SELECT ID,COALESCE(CODE,''),TYPE FROM REQ_TYPE"))
    return ReferenceDataResult::failure(duplicate.lastError().text());
  while (duplicate.next()) {
    if (duplicate.value(0).toInt() == id) continue;
    if (duplicate.value(2).toString().trimmed().compare(label, Qt::CaseInsensitive) == 0)
      return ReferenceDataResult::failure("Un type d'exigence porte déjà ce libellé.");
    if (!code.isEmpty() && duplicate.value(1).toString().trimmed().compare(code, Qt::CaseInsensitive) == 0)
      return ReferenceDataResult::failure("Un type d'exigence porte déjà ce code.");
  }
  QSqlQuery query(db);
  if (id < 0) query.prepare("INSERT INTO REQ_TYPE(CODE,TYPE) VALUES(?,?)");
  else query.prepare("UPDATE REQ_TYPE SET CODE=?,TYPE=? WHERE ID=?");
  query.addBindValue(code.isEmpty() ? QVariant() : QVariant(code));
  query.addBindValue(label); if (id >= 0) query.addBindValue(id);
  if (!query.exec()) return ReferenceDataResult::failure(query.lastError().text());
  return ReferenceDataResult::ok(id < 0 ? query.lastInsertId().toInt() : id);
}

ReferenceDataResult ReferenceDataService::deleteVerificationMethod(int id) const {
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen()) return unavailable(db);
  QSqlQuery used(db);
  used.prepare("SELECT (SELECT COUNT(*) FROM REQUIREMENT WHERE VERIF_METHOD=?) + (SELECT COUNT(*) FROM REQUIREMENT_VERIFICATION WHERE METHOD_ID=?)");
  used.addBindValue(id); used.addBindValue(id);
  if (!used.exec() || !used.next()) return ReferenceDataResult::failure(used.lastError().text());
  if (used.value(0).toInt() > 0) return ReferenceDataResult::failure("Cette méthode est utilisée par au moins une exigence et ne peut pas être supprimée.");
  QSqlQuery query(db); query.prepare("DELETE FROM REQ_METHOD WHERE ID=?"); query.addBindValue(id);
  if (!query.exec()) return ReferenceDataResult::failure(query.lastError().text());
  return query.numRowsAffected() == 1 ? ReferenceDataResult::ok(id) : ReferenceDataResult::failure("Méthode de vérification introuvable.");
}

ReferenceDataResult ReferenceDataService::deleteRequirementType(int id) const {
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen()) return unavailable(db);
  QSqlQuery used(db); used.prepare("SELECT COUNT(*) FROM REQUIREMENT WHERE TYPE=?"); used.addBindValue(id);
  if (!used.exec() || !used.next()) return ReferenceDataResult::failure(used.lastError().text());
  if (used.value(0).toInt() > 0) return ReferenceDataResult::failure("Ce type est utilisé par au moins une exigence et ne peut pas être supprimé.");
  QSqlQuery query(db); query.prepare("DELETE FROM REQ_TYPE WHERE ID=?"); query.addBindValue(id);
  if (!query.exec()) return ReferenceDataResult::failure(query.lastError().text());
  return query.numRowsAffected() == 1 ? ReferenceDataResult::ok(id) : ReferenceDataResult::failure("Type d'exigence introuvable.");
}
