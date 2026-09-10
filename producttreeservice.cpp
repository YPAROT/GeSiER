#include "producttreeservice.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QVariant nullId(int id) { return id < 0 ? QVariant() : QVariant(id); }
bool event(QSqlDatabase db, const QString &t, int id,
           const QString &before = {}, const QString &after = {}) {
  QSqlQuery q(db);
  q.prepare("INSERT INTO "
            "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON)"
            " VALUES(?,'PT',?,?,?)");
  q.addBindValue(t);
  q.addBindValue(id);
  q.addBindValue(before);
  q.addBindValue(after);
  return q.exec();
}
QString sep(QSqlDatabase db) {
  QSqlQuery q("SELECT VALUE FROM PROJECT_META WHERE KEY='pt_separator'", db);
  return q.next() && !q.value(0).toString().isEmpty() ? q.value(0).toString()
                                                      : "-";
}
} // namespace
ProductTreeService::ProductTreeService(QString n) : m_connectionName(n) {}
void ProductTreeService::setConnectionName(const QString &n) {
  m_connectionName = n;
}
ProductTreeResult ProductTreeService::fail(const QString &m) const {
  return {false, m, {}, {}};
}
QString ProductTreeService::fullCode(int id) const {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QSqlQuery q(db);
  q.prepare(
      "WITH RECURSIVE P(ID,SEGMENT,PARENT,L) AS(SELECT ID,SEGMENT,PARENT,0 "
      "FROM PT WHERE ID=? UNION ALL SELECT X.ID,X.SEGMENT,X.PARENT,P.L+1 FROM "
      "PT X JOIN P ON X.ID=P.PARENT) SELECT SEGMENT FROM P ORDER BY L DESC");
  q.addBindValue(id);
  q.exec();
  QStringList p;
  while (q.next())
    p << q.value(0).toString();
  return p.join(sep(db));
}
QStringList ProductTreeService::subtreeCodes(int id) const {
  QSqlQuery q(QSqlDatabase::database(m_connectionName));
  q.prepare("WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL SELECT P.ID FROM PT P "
            "JOIN D ON P.PARENT=D.ID) SELECT ID FROM D");
  q.addBindValue(id);
  q.exec();
  QStringList r;
  while (q.next())
    r << fullCode(q.value(0).toInt());
  return r;
}
static bool syncNames(QSqlDatabase db, const QString &cn, int root) {
  ProductTreeService s(cn);
  QSqlQuery q(db);
  q.prepare("WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL SELECT P.ID FROM PT P "
            "JOIN D ON P.PARENT=D.ID) SELECT ID FROM D");
  q.addBindValue(root);
  if (!q.exec())
    return false;
  QList<int> ids;
  while (q.next())
    ids << q.value(0).toInt();
  for (int id : ids) {
    QSqlQuery u(db);
    u.prepare("UPDATE PT SET NAME=? WHERE ID=?");
    u.addBindValue(s.fullCode(id));
    u.addBindValue(id);
    if (!u.exec())
      return false;
  }
  return true;
}
ProductTreeResult ProductTreeService::addNode(int parent, const QString &raw) {
  QString segment = raw.trimmed();
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  if (segment.isEmpty())
    return fail("Le segment est obligatoire.");
  QSqlQuery r("SELECT COUNT(*) FROM PT WHERE PARENT IS NULL AND ARCHIVED=0",
              db);
  r.next();
  if (parent < 0 && r.value(0).toInt())
    return fail("Le projet possède déjà une racine active.");
  if (!db.transaction())
    return fail(db.lastError().text());
  QSqlQuery q(db);
  q.prepare("INSERT INTO "
            "PT(NAME,PARENT,SEGMENT,POSITION,ARCHIVED,CREATED_AT,UPDATED_AT) "
            "VALUES(?,?,?,COALESCE((SELECT MAX(POSITION)+1 FROM PT WHERE "
            "PARENT IS ?),0),0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
  q.addBindValue(segment);
  q.addBindValue(nullId(parent));
  q.addBindValue(segment);
  q.addBindValue(nullId(parent));
  if (!q.exec()) {
    db.rollback();
    return fail(q.lastError().text());
  }
  int id = q.lastInsertId().toInt();
  if (!syncNames(db, m_connectionName, id) ||
      !event(db, "CREATE", id, {}, fullCode(id)) || !db.commit()) {
    db.rollback();
    return fail(db.lastError().text());
  }
  return {true, "Élément ajouté.", {fullCode(id)}, {}};
}
ProductTreeResult ProductTreeService::updateNode(int id, const QString &raw,
                                                 const QString &description) {
  QString segment = raw.trimmed();
  if (segment.isEmpty())
    return fail("Le segment est obligatoire.");
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QString old = fullCode(id);
  if (!db.transaction())
    return fail(db.lastError().text());
  QSqlQuery q(db);
  q.prepare("UPDATE PT SET "
            "SEGMENT=?,DESCRIPTION=?,UPDATED_AT=CURRENT_TIMESTAMP WHERE ID=?");
  q.addBindValue(segment);
  q.addBindValue(description);
  q.addBindValue(id);
  if (!q.exec() || !syncNames(db, m_connectionName, id) ||
      !event(db, "UPDATE", id, old, fullCode(id)) || !db.commit()) {
    db.rollback();
    return fail(q.lastError().text());
  }
  return {true, "Élément modifié.", subtreeCodes(id), {}};
}
ProductTreeResult ProductTreeService::moveNode(int id, int parent,
                                               int position) {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  if (id == parent)
    return fail("Un élément ne peut pas être son propre parent.");
  QSqlQuery c(db);
  c.prepare("WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL SELECT P.ID FROM PT P "
            "JOIN D ON P.PARENT=D.ID) SELECT 1 FROM D WHERE ID=?");
  c.addBindValue(id);
  c.addBindValue(parent);
  if (c.exec() && c.next())
    return fail("Déplacement impossible : cycle détecté.");
  QSqlQuery i(db);
  i.prepare("SELECT PARENT,SEGMENT FROM PT WHERE ID=?");
  i.addBindValue(id);
  if (!i.exec() || !i.next())
    return fail("Élément introuvable.");
  int oldParent = i.value(0).isNull() ? -1 : i.value(0).toInt();
  QString segment = i.value(1).toString(), oldCode = fullCode(id);
  if (parent < 0 && oldParent >= 0)
    return fail("Une seconde racine est interdite.");
  QSqlQuery d(db);
  d.prepare("SELECT 1 FROM PT WHERE ID<>? AND PARENT IS ? AND SEGMENT=?");
  d.addBindValue(id);
  d.addBindValue(nullId(parent));
  d.addBindValue(segment);
  if (d.exec() && d.next())
    return fail("Ce parent possède déjà un enfant avec ce segment.");
  if (!db.transaction())
    return fail(db.lastError().text());
  QSqlQuery s(db);
  s.prepare("UPDATE PT SET POSITION=POSITION+1 WHERE PARENT IS ? AND ID<>? AND "
            "POSITION>=?");
  s.addBindValue(nullId(parent));
  s.addBindValue(id);
  s.addBindValue(qMax(0, position));
  QSqlQuery m(db);
  m.prepare("UPDATE PT SET PARENT=?,POSITION=?,UPDATED_AT=CURRENT_TIMESTAMP "
            "WHERE ID=?");
  m.addBindValue(nullId(parent));
  m.addBindValue(qMax(0, position));
  m.addBindValue(id);
  if (!s.exec() || !m.exec() || !syncNames(db, m_connectionName, id) ||
      !event(db, oldParent == parent ? "REORDER" : "MOVE", id, oldCode,
             fullCode(id)) ||
      !db.commit()) {
    db.rollback();
    return fail(m.lastError().text());
  }
  return {true,
          oldParent == parent ? "Ordre modifié." : "Branche déplacée.",
          subtreeCodes(id),
          {}};
}
ProductTreeResult ProductTreeService::setArchived(int id, bool archived) {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  if (!db.transaction())
    return fail(db.lastError().text());
  QSqlQuery q(db);
  q.prepare(
      "WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL SELECT P.ID FROM PT P JOIN D "
      "ON P.PARENT=D.ID) UPDATE PT SET ARCHIVED=?,UPDATED_AT=CURRENT_TIMESTAMP "
      "WHERE ID IN(SELECT ID FROM D)");
  q.addBindValue(id);
  q.addBindValue(archived);
  if (!q.exec() || !event(db, archived ? "ARCHIVE" : "RESTORE", id) ||
      !db.commit()) {
    db.rollback();
    return fail(q.lastError().text());
  }
  return {true,
          archived ? "Branche archivée." : "Branche restaurée.",
          subtreeCodes(id),
          {}};
}
ProductTreeResult ProductTreeService::removeNode(int id) {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QSqlQuery b(db);
  b.prepare(
      "WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL SELECT P.ID FROM PT P JOIN D "
      "ON P.PARENT=D.ID) SELECT 'Exigence '||R.CODE FROM REQUIREMENT_PT X JOIN "
      "REQUIREMENT R ON R.ID=X.REQ_ID WHERE X.PT_ID IN(SELECT ID FROM D) UNION "
      "SELECT 'Interface '||COALESCE(I.CODE,CAST(I.ID AS TEXT)) FROM INTERFACE "
      "I WHERE I.ELEMENT1 IN(SELECT ID FROM D) OR I.ELEMENT2 IN(SELECT ID FROM "
      "D) UNION SELECT 'Changement '||C.CODE FROM CHANGE_LINK L JOIN "
      "CHANGE_ITEM C ON C.ID=L.CHANGE_ID WHERE L.OBJECT_TYPE='PT' AND "
      "L.OBJECT_ID IN(SELECT ID FROM D)");
  b.addBindValue(id);
  if (!b.exec())
    return fail(b.lastError().text());
  QStringList blockers;
  while (b.next())
    blockers << b.value(0).toString();
  if (!blockers.isEmpty()) {
    auto r = fail("La branche est utilisée et ne peut pas être supprimée.");
    r.blockers = blockers;
    return r;
  }
  QString code = fullCode(id);
  if (!db.transaction())
    return fail(db.lastError().text());
  QSqlQuery q(db);
  q.prepare(
      "DELETE FROM PT WHERE ID IN(WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL "
      "SELECT P.ID FROM PT P JOIN D ON P.PARENT=D.ID) SELECT ID FROM D)");
  q.addBindValue(id);
  if (!q.exec() || !event(db, "DELETE", id, code) || !db.commit()) {
    db.rollback();
    return fail(q.lastError().text());
  }
  return {true, "Branche supprimée.", {}, {}};
}
