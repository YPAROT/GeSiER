#include "changeservice.h"
#include "tabularservice.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

namespace {
QSqlDatabase dbFor(const QString &n) {
  return QSqlDatabase::contains(n) ? QSqlDatabase::database(n, false)
                                   : QSqlDatabase();
}
QString snapshot(const ChangeRecord &r) {
  QJsonArray a;
  for (const auto &l : r.links)
    a.append(QJsonObject{{"type", l.objectType}, {"id", l.objectId}});
  return QString::fromUtf8(
      QJsonDocument(QJsonObject{{"code", r.code},
                                {"type", r.typeId},
                                {"status", r.statusId},
                                {"description", r.description},
                                {"decision", r.decision},
                                {"opened", r.openedAt},
                                {"closed", r.closedAt},
                                {"reference", r.externalReference},
                                {"link", r.externalLink},
                                {"archived", r.archived},
                                {"links", a}})
          .toJson(QJsonDocument::Compact));
}
QString linkLabel(QSqlDatabase db, const QString &t, int id) {
  QString sql;
  if (t == "REQUIREMENT")
    sql = "SELECT CODE||' — '||TITLE FROM REQUIREMENT WHERE ID=?";
  else if (t == "PT")
    sql = "SELECT NAME FROM PT WHERE ID=?";
  else if (t == "CONFIGURATION")
    sql = "SELECT CODE||' — '||LABEL FROM CONFIGURATION WHERE ID=?";
  else if (t == "INTERFACE")
    sql = "SELECT COALESCE(CODE,'IF-'||ID) FROM INTERFACE WHERE ID=?";
  else if (t == "DOCUMENT")
    sql =
        "SELECT COALESCE(NULLIF(REFERENCE,''),TITLE) FROM DOCUMENT WHERE ID=?";
  else
    return {};
  QSqlQuery q(db);
  q.prepare(sql);
  q.addBindValue(id);
  return q.exec() && q.next() ? q.value(0).toString() : QString();
}
bool writeBook(const QString &p,
               const QList<QPair<QString, QList<QStringList>>> &ss,
               QString *e) {
  QList<TabularSheet> normalized; for (const auto &sheet : ss) normalized << TabularSheet{sheet.first, sheet.second};
  return TabularService::writeXlsx(p, normalized, e);
#if 0
  QZipWriter z(p);
  if (z.status() != QZipWriter::NoError) {
    *e = "Impossible de créer le classeur.";
    return false;
  }
  QByteArray types =
      "<?xml version=\"1.0\"?><Types "
      "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
      "content-types\"><Default Extension=\"rels\" "
      "ContentType=\"application/"
      "vnd.openxmlformats-package.relationships+xml\"/><Default "
      "Extension=\"xml\" ContentType=\"application/xml\"/><Override "
      "PartName=\"/xl/workbook.xml\" "
      "ContentType=\"application/"
      "vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>";
  for (int i = 0; i < ss.size(); ++i)
    types +=
        "<Override PartName=\"/xl/worksheets/sheet" +
        QByteArray::number(i + 1) +
        ".xml\" "
        "ContentType=\"application/"
        "vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
  types += "</Types>";
  z.addFile("[Content_Types].xml", types);
  z.addFile("_rels/.rels", "<?xml version=\"1.0\"?><Relationships "
                           "xmlns=\"http://schemas.openxmlformats.org/package/"
                           "2006/relationships\"><Relationship Id=\"rId1\" "
                           "Type=\"http://schemas.openxmlformats.org/"
                           "officeDocument/2006/relationships/officeDocument\" "
                           "Target=\"xl/workbook.xml\"/></Relationships>");
  QByteArray
      wb =
          "<?xml version=\"1.0\"?><workbook "
          "xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
          "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/"
          "relationships\"><sheets>",
      rels = "<?xml version=\"1.0\"?><Relationships "
             "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
             "relationships\">";
  for (int i = 0; i < ss.size(); ++i) {
    wb += "<sheet name=\"" + ss[i].first.toHtmlEscaped().toUtf8() +
          "\" sheetId=\"" + QByteArray::number(i + 1) + "\" r:id=\"rId" +
          QByteArray::number(i + 1) + "\"/>";
    rels += "<Relationship Id=\"rId" + QByteArray::number(i + 1) +
            "\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
            "relationships/worksheet\" Target=\"worksheets/sheet" +
            QByteArray::number(i + 1) + ".xml\"/>";
    z.addFile("xl/worksheets/sheet" + QByteArray::number(i + 1) + ".xml",
              sheet(ss[i].second));
  }
  wb += "</sheets></workbook>";
  rels += "</Relationships>";
  z.addFile("xl/workbook.xml", wb);
  z.addFile("xl/_rels/workbook.xml.rels", rels);
  z.close();
  if (z.status() != QZipWriter::NoError || QFileInfo(p).size() == 0) {
    *e = "Échec de finalisation du classeur.";
    return false;
  }
  return true;
#endif
}
} // namespace

ChangeService::ChangeService(QString n) : m_connectionName(std::move(n)) {}
void ChangeService::setConnectionName(const QString &n) {
  m_connectionName = n;
}
QList<ChangeRecord> ChangeService::find(const ChangeFilter &f,
                                        QString *error) const {
  QList<ChangeRecord> out;
  auto db = dbFor(m_connectionName);
  if (!db.isValid() || !db.isOpen()) {
    if (error)
      *error = "La base de données n'est pas ouverte.";
    return out;
  }
  QString sql =
      "SELECT "
      "C.ID,C.CODE,C.TYPE_ID,C.STATUS_ID,T.CODE,T.LABEL,S.CODE,S.LABEL,S.IS_"
      "FINAL,COALESCE(C.DESCRIPTION,''),COALESCE(C.DECISION,''),COALESCE(C."
      "OPENED_AT,''),COALESCE(C.CLOSED_AT,''),COALESCE(C.EXTERNAL_REFERENCE,'')"
      ",COALESCE(C.EXTERNAL_LINK,''),COALESCE(C.ARCHIVED,0) FROM CHANGE_ITEM C "
      "JOIN CHANGE_TYPE T ON T.ID=C.TYPE_ID JOIN CHANGE_STATUS S ON "
      "S.ID=C.STATUS_ID WHERE 1=1";
  QVariantList b;
  if (!f.includeArchived)
    sql += " AND C.ARCHIVED=0";
  if (f.typeId >= 0) {
    sql += " AND C.TYPE_ID=?";
    b << f.typeId;
  }
  if (f.statusId >= 0) {
    sql += " AND C.STATUS_ID=?";
    b << f.statusId;
  }
  if (f.onlyIncomplete)
    sql += " AND (TRIM(C.DECISION)='' OR S.IS_FINAL=0)";
  if (f.objectId >= 0 && !f.objectType.isEmpty()) {
    if (f.objectType == "PT") {
      sql += " AND EXISTS(SELECT 1 FROM CHANGE_LINK L JOIN REQUIREMENT_PT P "
             "ON P.REQ_ID=L.OBJECT_ID WHERE L.CHANGE_ID=C.ID AND "
             "L.OBJECT_TYPE='REQUIREMENT' AND P.PT_ID=?)";
      b << f.objectId;
    } else if (f.objectType == "CONFIGURATION") {
      sql += " AND EXISTS(SELECT 1 FROM CHANGE_LINK L JOIN "
             "REQUIREMENT_APPLICABILITY A ON A.REQ_ID=L.OBJECT_ID WHERE "
             "L.CHANGE_ID=C.ID AND L.OBJECT_TYPE='REQUIREMENT' AND "
             "A.CONFIG_ID=?)";
      b << f.objectId;
    } else if (f.objectType == "INTERFACE") {
      sql += " AND EXISTS(SELECT 1 FROM CHANGE_LINK L JOIN "
             "INTERFACE_REQUIREMENT I ON I.REQ_ID=L.OBJECT_ID WHERE "
             "L.CHANGE_ID=C.ID AND L.OBJECT_TYPE='REQUIREMENT' AND "
             "I.INTERFACE_ID=?)";
      b << f.objectId;
    } else if (f.objectType == "DOCUMENT") {
      sql += " AND EXISTS(SELECT 1 FROM CHANGE_LINK L JOIN DOCUMENT_NODE N "
             "ON N.REQ_ID=L.OBJECT_ID WHERE L.CHANGE_ID=C.ID AND "
             "L.OBJECT_TYPE='REQUIREMENT' AND N.DOC_ID=?)";
      b << f.objectId;
    } else {
      sql += " AND EXISTS(SELECT 1 FROM CHANGE_LINK L WHERE L.CHANGE_ID=C.ID "
             "AND L.OBJECT_TYPE='REQUIREMENT' AND L.OBJECT_ID=?)";
      b << f.objectId;
    }
  }
  if (!f.text.trimmed().isEmpty()) {
    sql += " AND (C.CODE LIKE ? OR C.DESCRIPTION LIKE ? OR C.DECISION LIKE ? "
           "OR C.EXTERNAL_REFERENCE LIKE ?)";
    QString s = "%" + f.text.trimmed() + "%";
    b << s << s << s << s;
  }
  sql += " ORDER BY C.ARCHIVED,S.IS_FINAL,C.CODE";
  QSqlQuery q(db);
  q.prepare(sql);
  for (auto &v : b)
    q.addBindValue(v);
  if (!q.exec()) {
    if (error)
      *error = q.lastError().text();
    return out;
  }
  while (q.next()) {
    ChangeRecord r;
    r.id = q.value(0).toInt();
    r.code = q.value(1).toString();
    r.typeId = q.value(2).toInt();
    r.statusId = q.value(3).toInt();
    r.typeCode = q.value(4).toString();
    r.typeLabel = q.value(5).toString();
    r.statusCode = q.value(6).toString();
    r.statusLabel = q.value(7).toString();
    r.finalStatus = q.value(8).toBool();
    r.description = q.value(9).toString();
    r.decision = q.value(10).toString();
    r.openedAt = q.value(11).toString();
    r.closedAt = q.value(12).toString();
    r.externalReference = q.value(13).toString();
    r.externalLink = q.value(14).toString();
    r.archived = q.value(15).toBool();
    out << r;
  }
  return out;
}
ChangeRecord ChangeService::get(int id, QString *error) const {
  ChangeFilter f;
  f.includeArchived = true;
  for (auto r : find(f, error))
    if (r.id == id) {
      QSqlQuery q(dbFor(m_connectionName));
      q.prepare("SELECT OBJECT_TYPE,OBJECT_ID FROM CHANGE_LINK WHERE "
                "CHANGE_ID=? ORDER BY OBJECT_TYPE,OBJECT_ID");
      q.addBindValue(id);
      q.exec();
      while (q.next()) {
        ChangeLink l{q.value(0).toString(), {}, q.value(1).toInt()};
        l.label = linkLabel(dbFor(m_connectionName), l.objectType, l.objectId);
        r.links << l;
      }
      return r;
    }
  if (error && error->isEmpty())
    *error = "Changement introuvable.";
  return {};
}
RequirementResult ChangeService::save(const ChangeRecord &r) {
  if (r.code.trimmed().isEmpty() || r.typeId < 0 || r.statusId < 0)
    return RequirementResult::failure(
        "Identifiant, type et statut sont obligatoires.");
  auto db = dbFor(m_connectionName);
  if (!db.isValid() || !db.isOpen())
    return RequirementResult::failure("La base de données n'est pas ouverte.");
  QSqlQuery status(db);
  status.prepare("SELECT IS_FINAL,CODE FROM CHANGE_STATUS WHERE ID=?");
  status.addBindValue(r.statusId);
  if (!status.exec() || !status.next())
    return RequirementResult::failure("Statut invalide.");
  bool final = status.value(0).toBool();
  QString statusCode = status.value(1).toString();
  if (final && r.decision.trimmed().isEmpty())
    return RequirementResult::failure(
        "Une décision est obligatoire pour un statut final.");
  if (!r.externalLink.trimmed().isEmpty()) {
    QUrl u = QUrl::fromUserInput(r.externalLink);
    if (!u.isValid() || u.scheme().isEmpty())
      return RequirementResult::failure("Le lien externe n'est pas valide.");
  }
  ChangeRecord before;
  if (r.id >= 0)
    before = get(r.id);
  QSet<QString> unique;
  for (const auto &l : r.links) {
    QString key = l.objectType + ":" + QString::number(l.objectId);
    if (unique.contains(key))
      continue;
    unique << key;
    if (l.objectType != "REQUIREMENT") {
      bool legacy = false;
      for (const auto &old : before.links)
        if (old.objectType == l.objectType && old.objectId == l.objectId) {
          legacy = true;
          break;
        }
      if (!legacy)
        return RequirementResult::failure(
            "Un changement ne peut être associé qu'à des exigences.");
    }
    if (linkLabel(db, l.objectType, l.objectId).isEmpty())
      return RequirementResult::failure("Un objet associé n'existe plus : " +
                                        key);
  }
  if (!db.transaction())
    return RequirementResult::failure(db.lastError().text());
  QSqlQuery q(db);
  if (r.id < 0)
    q.prepare(
        "INSERT INTO "
        "CHANGE_ITEM(CODE,TYPE,STATUS,TYPE_ID,STATUS_ID,DESCRIPTION,DECISION,"
        "OPENED_AT,CLOSED_AT,EXTERNAL_REFERENCE,EXTERNAL_LINK,ARCHIVED,UPDATED_"
        "AT) VALUES(?,(SELECT CASE WHEN CODE "
        "IN('CHANGE_REQUEST','WAIVER','DEVIATION') THEN CODE ELSE 'OTHER' END "
        "FROM CHANGE_TYPE WHERE ID=?),?,?,?,?,?,?,?,?,?,0,CURRENT_TIMESTAMP)");
  else
    q.prepare("UPDATE CHANGE_ITEM SET CODE=?,TYPE=(SELECT CASE WHEN CODE "
              "IN('CHANGE_REQUEST','WAIVER','DEVIATION') THEN CODE ELSE "
              "'OTHER' END FROM CHANGE_TYPE WHERE "
              "ID=?),STATUS=?,TYPE_ID=?,STATUS_ID=?,DESCRIPTION=?,DECISION=?,"
              "OPENED_AT=?,CLOSED_AT=?,EXTERNAL_REFERENCE=?,EXTERNAL_LINK=?,"
              "UPDATED_AT=CURRENT_TIMESTAMP WHERE ID=?");
  q.addBindValue(r.code.trimmed());
  q.addBindValue(r.typeId);
  q.addBindValue(statusCode);
  q.addBindValue(r.typeId);
  q.addBindValue(r.statusId);
  q.addBindValue(r.description);
  q.addBindValue(r.decision);
  q.addBindValue(r.openedAt.trimmed().isEmpty()
                     ? QVariant(QDateTime::currentDateTime().toString(Qt::ISODate))
                     : QVariant(r.openedAt));
  q.addBindValue(
      final
          ? (r.closedAt.trimmed().isEmpty()
                 ? QVariant(QDateTime::currentDateTime().toString(Qt::ISODate))
                 : QVariant(r.closedAt))
          : QVariant());
  q.addBindValue(r.externalReference);
  q.addBindValue(r.externalLink);
  if (r.id >= 0)
    q.addBindValue(r.id);
  if (!q.exec()) {
    db.rollback();
    return RequirementResult::failure(q.lastError().text());
  }
  int id = r.id < 0 ? q.lastInsertId().toInt() : r.id;
  QSqlQuery d(db);
  d.prepare("DELETE FROM CHANGE_LINK WHERE CHANGE_ID=?");
  d.addBindValue(id);
  if (!d.exec()) {
    db.rollback();
    return RequirementResult::failure(d.lastError().text());
  }
  for (const auto &l : r.links) {
    QString key = l.objectType + ":" + QString::number(l.objectId);
    if (!unique.remove(key))
      continue;
    QSqlQuery i(db);
    i.prepare("INSERT INTO CHANGE_LINK(CHANGE_ID,OBJECT_TYPE,OBJECT_ID) "
              "VALUES(?,?,?)");
    i.addBindValue(id);
    i.addBindValue(l.objectType);
    i.addBindValue(l.objectId);
    if (!i.exec()) {
      db.rollback();
      return RequirementResult::failure(i.lastError().text());
    }
  }
  ChangeRecord after = r;
  after.id = id;
  QSqlQuery log(db);
  log.prepare("INSERT INTO "
              "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_"
              "JSON) VALUES(?,?,?,?,?)");
  log.addBindValue(r.id < 0 ? "CREATE" : "UPDATE");
  log.addBindValue("CHANGE");
  log.addBindValue(id);
  log.addBindValue(r.id < 0 ? QVariant() : QVariant(snapshot(before)));
  log.addBindValue(snapshot(after));
  if (!log.exec() || !db.commit()) {
    db.rollback();
    return RequirementResult::failure(log.lastError().text().isEmpty()
                                          ? db.lastError().text()
                                          : log.lastError().text());
  }
  return RequirementResult::successResult("Changement enregistré.", id, r.code);
}
RequirementResult ChangeService::setArchived(int id, bool a) {
  auto db = dbFor(m_connectionName);
  QSqlQuery q(db);
  q.prepare("UPDATE CHANGE_ITEM SET ARCHIVED=?,UPDATED_AT=CURRENT_TIMESTAMP "
            "WHERE ID=?");
  q.addBindValue(a);
  q.addBindValue(id);
  if (!q.exec() || q.numRowsAffected() != 1)
    return RequirementResult::failure(q.lastError().text().isEmpty()
                                          ? "Changement introuvable."
                                          : q.lastError().text());
  QSqlQuery l(db);
  l.prepare(
      "INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
      "VALUES(?,?,?,?)");
  l.addBindValue(a ? "ARCHIVE" : "RESTORE");
  l.addBindValue("CHANGE");
  l.addBindValue(id);
  l.addBindValue(QString("{\"archived\":%1}").arg(a ? "true" : "false"));
  l.exec();
  return RequirementResult::successResult(
      a ? "Changement archivé." : "Changement restauré.", id);
}
RequirementResult ChangeService::saveType(int id, const QString &c,
                                          const QString &l, bool active) {
  if (c.trimmed().isEmpty() || l.trimmed().isEmpty())
    return RequirementResult::failure("Code et libellé sont obligatoires.");
  QSqlQuery q(dbFor(m_connectionName));
  q.prepare(
      id < 0
          ? "INSERT INTO CHANGE_TYPE(CODE,LABEL,ACTIVE) VALUES(UPPER(?),?,?)"
          : "UPDATE CHANGE_TYPE SET CODE=UPPER(?),LABEL=?,ACTIVE=? WHERE ID=?");
  q.addBindValue(c.trimmed());
  q.addBindValue(l.trimmed());
  q.addBindValue(active);
  if (id >= 0)
    q.addBindValue(id);
  if (!q.exec())
    return RequirementResult::failure(q.lastError().text());
  return RequirementResult::successResult(
      "Type enregistré.", id < 0 ? q.lastInsertId().toInt() : id);
}
RequirementResult ChangeService::saveStatus(int id, const QString &c,
                                            const QString &l, bool final,
                                            int pos) {
  if (c.trimmed().isEmpty() || l.trimmed().isEmpty())
    return RequirementResult::failure("Code et libellé sont obligatoires.");
  QSqlQuery q(dbFor(m_connectionName));
  q.prepare(id < 0 ? "INSERT INTO CHANGE_STATUS(CODE,LABEL,IS_FINAL,POSITION) "
                     "VALUES(UPPER(?),?,?,?)"
                   : "UPDATE CHANGE_STATUS SET "
                     "CODE=UPPER(?),LABEL=?,IS_FINAL=?,POSITION=? WHERE ID=?");
  q.addBindValue(c.trimmed());
  q.addBindValue(l.trimmed());
  q.addBindValue(final);
  q.addBindValue(pos);
  if (id >= 0)
    q.addBindValue(id);
  if (!q.exec())
    return RequirementResult::failure(q.lastError().text());
  return RequirementResult::successResult(
      "Statut enregistré.", id < 0 ? q.lastInsertId().toInt() : id);
}
RequirementResult ChangeService::removeCatalogValue(const QString &t, int id) {
  if (t != "CHANGE_TYPE" && t != "CHANGE_STATUS")
    return RequirementResult::failure("Catalogue invalide.");
  QString column = t == "CHANGE_TYPE" ? "TYPE_ID" : "STATUS_ID";
  QSqlQuery c(dbFor(m_connectionName));
  c.prepare("SELECT COUNT(*) FROM CHANGE_ITEM WHERE " + column + "=?");
  c.addBindValue(id);
  c.exec();
  c.next();
  if (c.value(0).toInt())
    return RequirementResult::failure(
        "Cette valeur est utilisée et ne peut pas être supprimée.");
  QSqlQuery q(dbFor(m_connectionName));
  q.prepare("DELETE FROM " + t + " WHERE ID=?");
  q.addBindValue(id);
  return q.exec() ? RequirementResult::successResult("Valeur supprimée.", id)
                  : RequirementResult::failure(q.lastError().text());
}
ChangeCoverage ChangeService::coverage(const ChangeFilter &f,
                                       QString *error) const {
  ChangeCoverage c;
  auto rows = find(f, error);
  c.total = rows.size();
  for (const auto &r : rows) {
    if (!r.decision.trimmed().isEmpty())
      c.decided++;
    if (r.finalStatus)
      c.final++;
    if (r.finalStatus && !r.decision.trimmed().isEmpty())
      c.complete++;
  }
  return c;
}
RequirementResult ChangeService::exportXlsx(const QString &p,
                                            const ChangeFilter &f) const {
  QString e;
  auto rows = find(f, &e);
  if (!e.isEmpty())
    return RequirementResult::failure(e);
  auto c = coverage(f);
  QList<QStringList> summary{
      {"Indicateur", "Valeur"},
      {"Objets", QString::number(c.total)},
      {"Avec décision", QString::number(c.decided)},
      {"Statut final", QString::number(c.final)},
      {"Couverture complète", QString::number(c.complete)},
      {"Taux de couverture",
       c.total ? QString::number(100.0 * c.complete / c.total, 'f', 2) + " %"
               : "0 %"}},
      details{{"Identifiant", "Type", "Statut", "Décision", "Ouverture",
               "Clôture", "Référence externe", "Lien externe", "Description",
               "État"}},
      links{{"Identifiant", "Type d'objet", "Objet"}};
  for (auto &r : rows) {
    details << QStringList{r.code,
                           r.typeLabel,
                           r.statusLabel,
                           r.decision,
                           r.openedAt,
                           r.closedAt,
                           r.externalReference,
                           r.externalLink,
                           r.description,
                           r.archived ? "Archivé" : "Actif"};
    auto full = get(r.id);
    for (auto &l : full.links)
      links << QStringList{r.code, l.objectType, l.label};
  }
  if (!writeBook(p,
                 {{"Synthèse", summary},
                  {"Changements", details},
                  {"Associations", links}},
                 &e))
    return RequirementResult::failure(e);
  return RequirementResult::successResult("Classeur XLSX exporté : " + p, -1);
}
