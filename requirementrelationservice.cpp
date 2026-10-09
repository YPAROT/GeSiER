#include "requirementrelationservice.h"
#include "auditservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

RequirementRelationService::RequirementRelationService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

QList<RequirementRelationRecord>
RequirementRelationService::relations(int requirementId, QString *error) const {
  QList<RequirementRelationRecord> result;
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare(
      "SELECT L.ID,L.SOURCE_REQ_ID,L.TARGET_REQ_ID,L.TYPE_ID,T.CODE,"
      "CASE WHEN L.SOURCE_REQ_ID=? THEN B.CODE ELSE A.CODE END,"
      "CASE WHEN L.SOURCE_REQ_ID=? THEN B.TITLE ELSE A.TITLE END,L.COMMENT,"
      "L.SOURCE_REQ_ID=?,"
      "CASE WHEN L.SOURCE_REQ_ID=? THEN BS.STATUS ELSE AS_.STATUS END,"
      "CASE WHEN L.SOURCE_REQ_ID=? THEN BT.TYPE ELSE AT.TYPE END,"
      "CASE WHEN L.SOURCE_REQ_ID=? THEN (SELECT GROUP_CONCAT(P.NAME,', ') FROM REQUIREMENT_PT RP JOIN PT P ON P.ID=RP.PT_ID WHERE RP.REQ_ID=B.ID) ELSE (SELECT GROUP_CONCAT(P.NAME,', ') FROM REQUIREMENT_PT RP JOIN PT P ON P.ID=RP.PT_ID WHERE RP.REQ_ID=A.ID) END "
      "FROM REQUIREMENT_RELATION L "
      "JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=L.TYPE_ID "
      "JOIN REQUIREMENT A ON A.ID=L.SOURCE_REQ_ID "
      "JOIN REQUIREMENT B ON B.ID=L.TARGET_REQ_ID "
      "LEFT JOIN REQ_STATUS AS_ ON AS_.ID=A.STATUS LEFT JOIN REQ_STATUS BS ON BS.ID=B.STATUS "
      "LEFT JOIN REQ_TYPE AT ON AT.ID=A.TYPE LEFT JOIN REQ_TYPE BT ON BT.ID=B.TYPE "
      "WHERE L.SOURCE_REQ_ID=? OR L.TARGET_REQ_ID=? ORDER BY T.ID,6");
  query.addBindValue(requirementId);
  query.addBindValue(requirementId);
  query.addBindValue(requirementId);
  query.addBindValue(requirementId);
  query.addBindValue(requirementId);
  query.addBindValue(requirementId);
  query.addBindValue(requirementId);
  query.addBindValue(requirementId);
  if (!query.exec()) {
    if (error)
      *error = query.lastError().text();
    return result;
  }
  while (query.next()) {
    RequirementRelationRecord record;
    record.id = query.value(0).toInt();
    record.sourceId = query.value(1).toInt();
    record.targetId = query.value(2).toInt();
    record.typeId = query.value(3).toInt();
    record.typeCode = query.value(4).toString();
    record.otherCode = query.value(5).toString();
    record.otherTitle = query.value(6).toString();
    record.comment = query.value(7).toString();
    record.outgoing = query.value(8).toBool();
    record.otherStatus = query.value(9).toString();
    record.otherType = query.value(10).toString();
    record.otherProductTrees = query.value(11).toString();
    result << record;
  }
  return result;
}

RequirementResult RequirementRelationService::updateComment(
    int relationId, const QString &comment) {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QSqlQuery endpoints(db);endpoints.prepare("SELECT SOURCE_REQ_ID,TARGET_REQ_ID FROM REQUIREMENT_RELATION WHERE ID=?");endpoints.addBindValue(relationId);if(!endpoints.exec()||!endpoints.next())return RequirementResult::failure("Relation introuvable.");const int source=endpoints.value(0).toInt(),target=endpoints.value(1).toInt();const auto before=AuditService::requirementSnapshot(db,source);
  if (!db.transaction())
    return RequirementResult::failure(db.lastError().text());
  QSqlQuery query(db);
  query.prepare("UPDATE REQUIREMENT_RELATION SET COMMENT=? WHERE ID=?");
  query.addBindValue(comment);
  query.addBindValue(relationId);
  if (!query.exec() || query.numRowsAffected() != 1) {
    const QString error = query.lastError().text().isEmpty()
                              ? QString("Relation introuvable.")
                              : query.lastError().text();
    db.rollback();
    return RequirementResult::failure(error);
  }
  QString auditError;QList<AuditSubject> subjects{{"REQUIREMENT_RELATION",relationId,"PRIMARY"},{"REQUIREMENT",source,"AFFECTED"},{"REQUIREMENT",target,"AFFECTED"}};
  if (!AuditService::record(db,"UPDATE","REQUIREMENT_RELATION",relationId,before,AuditService::requirementSnapshot(db,source),subjects,comment,&auditError) || !db.commit()) {
    const QString error = auditError.isEmpty() ? db.lastError().text() : auditError;
    db.rollback();
    return RequirementResult::failure(error);
  }
  return RequirementResult::successResult("Commentaire modifié.", relationId);
}

RequirementResult RequirementRelationService::add(int sourceId, int targetId,
                                                   int typeId,
                                                   const QString &comment) {
  if (sourceId < 0 || targetId < 0 || sourceId == targetId)
    return RequirementResult::failure("Une exigence ne peut pas être reliée à elle-même.");
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  const auto before=AuditService::requirementSnapshot(db,sourceId);
  if (!db.transaction())
    return RequirementResult::failure(db.lastError().text());
  QSqlQuery insert(db);
  insert.prepare("INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID,COMMENT) VALUES(?,?,?,?)");
  insert.addBindValue(sourceId);
  insert.addBindValue(targetId);
  insert.addBindValue(typeId);
  insert.addBindValue(comment);
  if (!insert.exec()) {
    const QString message = insert.lastError().text();
    db.rollback();
    return RequirementResult::failure(message);
  }
  const int id = insert.lastInsertId().toInt();
  QString auditError;QList<AuditSubject> subjects{{"REQUIREMENT_RELATION",id,"PRIMARY"},{"REQUIREMENT",sourceId,"AFFECTED"},{"REQUIREMENT",targetId,"AFFECTED"}};
  if (!AuditService::record(db,"CREATE","REQUIREMENT_RELATION",id,before,AuditService::requirementSnapshot(db,sourceId),subjects,comment,&auditError) || !db.commit()) {
    const QString message = db.lastError().text();
    db.rollback();
    return RequirementResult::failure(message);
  }
  return RequirementResult::successResult("Relation ajoutée.", id);
}

RequirementResult RequirementRelationService::remove(int relationId) {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QSqlQuery endpoints(db);endpoints.prepare("SELECT SOURCE_REQ_ID,TARGET_REQ_ID FROM REQUIREMENT_RELATION WHERE ID=?");endpoints.addBindValue(relationId);if(!endpoints.exec()||!endpoints.next())return RequirementResult::failure("Relation introuvable.");const int source=endpoints.value(0).toInt(),target=endpoints.value(1).toInt();const auto before=AuditService::requirementSnapshot(db,source);if(!db.transaction())return RequirementResult::failure(db.lastError().text());
  QSqlQuery query(db);
  query.prepare("DELETE FROM REQUIREMENT_RELATION WHERE ID=?");
  query.addBindValue(relationId);
  QString auditError;QList<AuditSubject> subjects{{"REQUIREMENT_RELATION",relationId,"PRIMARY"},{"REQUIREMENT",source,"AFFECTED"},{"REQUIREMENT",target,"AFFECTED"}};
  if (!query.exec()||!AuditService::record(db,"DELETE","REQUIREMENT_RELATION",relationId,before,AuditService::requirementSnapshot(db,source),subjects,{},&auditError)||!db.commit()){db.rollback();return RequirementResult::failure(auditError.isEmpty()?query.lastError().text():auditError);}
  return RequirementResult::successResult("Relation supprimée.", relationId);
}
