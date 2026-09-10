#include "documentservice.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

DocumentService::DocumentService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

QList<DocumentRecord> DocumentService::documents() const {
  QList<DocumentRecord> result;
  QSqlQuery query("SELECT ID,PT_ID,TYPE,COALESCE(REFERENCE,''),TITLE,"
                  "COALESCE(DESCRIPTION,'') FROM DOCUMENT ORDER BY TITLE",
                  QSqlDatabase::database(m_connectionName));
  while (query.next()) {
    DocumentRecord record;
    record.id = query.value(0).toInt();
    record.ptId = query.value(1).toInt();
    record.typeId = query.value(2).toInt();
    record.reference = query.value(3).toString();
    record.title = query.value(4).toString();
    record.description = query.value(5).toString();
    result << record;
  }
  return result;
}

DocumentRecord DocumentService::document(int id) const {
  for (DocumentRecord record : documents()) {
    if (record.id != id)
      continue;
    QSqlQuery references(QSqlDatabase::database(m_connectionName));
    references.prepare("SELECT REFERENCE FROM DOCUMENT_REFERENCE WHERE DOC_ID=? AND IS_PRIMARY=0 ORDER BY ID");
    references.addBindValue(id);
    if (references.exec())
      while (references.next())
        record.secondaryReferences << references.value(0).toString();
    return record;
  }
  return {};
}

QList<DocumentNodeRecord> DocumentService::nodes(int documentId) const {
  QList<DocumentNodeRecord> result;
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare("SELECT N.ID,N.DOC_ID,N.PARENT_ID,N.POSITION,N.NODE_TYPE,"
                "COALESCE(N.TITLE,''),N.REQ_ID,COALESCE(R.CODE,''),"
                "COALESCE(R.TITLE,'') "
                "FROM DOCUMENT_NODE N LEFT JOIN REQUIREMENT R ON R.ID=N.REQ_ID "
                "WHERE N.DOC_ID=? ORDER BY N.POSITION,N.ID");
  query.addBindValue(documentId);
  if (!query.exec())
    return result;
  while (query.next()) {
    DocumentNodeRecord node;
    node.id = query.value(0).toInt();
    node.documentId = query.value(1).toInt();
    node.parentId = query.value(2).isNull() ? -1 : query.value(2).toInt();
    node.position = query.value(3).toInt();
    node.type = query.value(4).toString();
    node.title = query.value(5).toString();
    node.requirementId = query.value(6).isNull() ? -1 : query.value(6).toInt();
    node.requirementCode = query.value(7).toString();
    node.requirementTitle = query.value(8).toString();
    result << node;
  }
  return result;
}

RequirementResult DocumentService::saveDocument(const DocumentRecord &record) {
  if (record.title.trimmed().isEmpty() || record.reference.trimmed().isEmpty())
    return RequirementResult::failure("La référence et le titre sont obligatoires.");
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  if (!db.transaction())
    return RequirementResult::failure(db.lastError().text());
  int id = record.id;
  QSqlQuery query(db);
  if (id < 0) {
    query.prepare("INSERT INTO DOCUMENT(PT_ID,TYPE,REFERENCE,TITLE,DESCRIPTION) VALUES(?,?,?,?,?)");
    query.addBindValue(record.ptId);
    query.addBindValue(record.typeId);
    query.addBindValue(record.reference.trimmed());
    query.addBindValue(record.title.trimmed());
    query.addBindValue(record.description);
    if (query.exec())
      id = query.lastInsertId().toInt();
  } else {
    query.prepare("UPDATE DOCUMENT SET PT_ID=?,TYPE=?,REFERENCE=?,TITLE=?,DESCRIPTION=? WHERE ID=?");
    query.addBindValue(record.ptId);
    query.addBindValue(record.typeId);
    query.addBindValue(record.reference.trimmed());
    query.addBindValue(record.title.trimmed());
    query.addBindValue(record.description);
    query.addBindValue(id);
    query.exec();
  }
  if (query.lastError().isValid()) {
    const QString message = query.lastError().text();
    db.rollback();
    return RequirementResult::failure(message);
  }
  QSqlQuery clear(db);
  clear.prepare("DELETE FROM DOCUMENT_REFERENCE WHERE DOC_ID=?");
  clear.addBindValue(id);
  if (!clear.exec()) {
    db.rollback();
    return RequirementResult::failure(clear.lastError().text());
  }
  QStringList references = record.secondaryReferences;
  references.prepend(record.reference.trimmed());
  for (int index = 0; index < references.size(); ++index) {
    if (references[index].trimmed().isEmpty())
      continue;
    QSqlQuery insert(db);
    insert.prepare("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(?,?,?)");
    insert.addBindValue(id);
    insert.addBindValue(references[index].trimmed());
    insert.addBindValue(index == 0);
    if (!insert.exec()) {
      db.rollback();
      return RequirementResult::failure(insert.lastError().text());
    }
  }
  if (!db.commit())
    return RequirementResult::failure(db.lastError().text());
  return RequirementResult::successResult("Document enregistré.", id);
}

RequirementResult DocumentService::addChapter(int documentId, int parentId,
                                              const QString &title) {
  if (title.trimmed().isEmpty())
    return RequirementResult::failure("Le titre du chapitre est obligatoire.");
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare("INSERT INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,NODE_TYPE,POSITION,TITLE) VALUES(?,?,'CHAPTER',(SELECT COUNT(*) FROM DOCUMENT_NODE WHERE DOC_ID=? AND PARENT_ID IS ?),?)");
  query.addBindValue(documentId);
  query.addBindValue(parentId < 0 ? QVariant() : QVariant(parentId));
  query.addBindValue(documentId);
  query.addBindValue(parentId < 0 ? QVariant() : QVariant(parentId));
  query.addBindValue(title.trimmed());
  if (!query.exec())
    return RequirementResult::failure(query.lastError().text());
  return RequirementResult::successResult("Chapitre ajouté.", query.lastInsertId().toInt());
}

RequirementResult DocumentService::placeRequirement(int documentId,
                                                    int parentId,
                                                    int requirementId) {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QSqlQuery existing(db);
  existing.prepare("SELECT ID FROM DOCUMENT_NODE WHERE DOC_ID=? AND REQ_ID=? AND NODE_TYPE='REQUIREMENT'");
  existing.addBindValue(documentId);
  existing.addBindValue(requirementId);
  if (existing.exec() && existing.next())
    return moveNode(existing.value(0).toInt(), parentId, -1);
  QSqlQuery query(db);
  query.prepare("INSERT INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,NODE_TYPE,POSITION,REQ_ID) VALUES(?,?,'REQUIREMENT',(SELECT COUNT(*) FROM DOCUMENT_NODE WHERE DOC_ID=? AND PARENT_ID IS ?),?)");
  query.addBindValue(documentId);
  query.addBindValue(parentId < 0 ? QVariant() : QVariant(parentId));
  query.addBindValue(documentId);
  query.addBindValue(parentId < 0 ? QVariant() : QVariant(parentId));
  query.addBindValue(requirementId);
  if (!query.exec())
    return RequirementResult::failure(query.lastError().text());
  return RequirementResult::successResult("Exigence placée.", query.lastInsertId().toInt());
}

RequirementResult DocumentService::moveNode(int nodeId, int parentId,
                                            int position) {
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare("UPDATE DOCUMENT_NODE SET PARENT_ID=?,POSITION=CASE WHEN ?<0 THEN (SELECT COUNT(*) FROM DOCUMENT_NODE S WHERE S.DOC_ID=DOCUMENT_NODE.DOC_ID AND S.PARENT_ID IS ?) ELSE ? END WHERE ID=?");
  const QVariant parent = parentId < 0 ? QVariant() : QVariant(parentId);
  query.addBindValue(parent);
  query.addBindValue(position);
  query.addBindValue(parent);
  query.addBindValue(position);
  query.addBindValue(nodeId);
  if (!query.exec())
    return RequirementResult::failure(query.lastError().text());
  return RequirementResult::successResult("Élément déplacé.", nodeId);
}

RequirementResult DocumentService::removeNode(int nodeId) {
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare("DELETE FROM DOCUMENT_NODE WHERE ID=?");
  query.addBindValue(nodeId);
  if (!query.exec())
    return RequirementResult::failure(query.lastError().text());
  return RequirementResult::successResult("Élément retiré.", nodeId);
}

RequirementResult DocumentService::renameChapter(int nodeId,
                                                 const QString &title) {
  if (title.trimmed().isEmpty())
    return RequirementResult::failure("Le titre du chapitre est obligatoire.");
  QSqlQuery query(QSqlDatabase::database(m_connectionName));
  query.prepare("UPDATE DOCUMENT_NODE SET TITLE=? WHERE ID=? AND NODE_TYPE='CHAPTER'");
  query.addBindValue(title.trimmed());
  query.addBindValue(nodeId);
  if (!query.exec())
    return RequirementResult::failure(query.lastError().text());
  return RequirementResult::successResult("Chapitre renommé.", nodeId);
}
