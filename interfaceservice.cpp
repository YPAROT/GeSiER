#include "interfaceservice.h"
#include "tabularservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QSqlDatabase database(const QString &name) {
  return QSqlDatabase::contains(name) ? QSqlDatabase::database(name, false)
                                      : QSqlDatabase();
}
bool workbook(const QString &path,
              const QList<QPair<QString, QList<QStringList>>> &sheets,
              QString *error) {
  QList<TabularSheet> normalized; for (const auto &sheet : sheets) normalized << TabularSheet{sheet.first, sheet.second};
  return TabularService::writeXlsx(path, normalized, error);
#if 0
  QZipWriter zip(path);
  if (zip.status() != QZipWriter::NoError) {
    if (error)
      *error = "Impossible de créer le classeur.";
    return false;
  }
  QByteArray types =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types "
      "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
      "content-types\"><Default Extension=\"rels\" "
      "ContentType=\"application/"
      "vnd.openxmlformats-package.relationships+xml\"/><Default "
      "Extension=\"xml\" ContentType=\"application/xml\"/><Override "
      "PartName=\"/xl/workbook.xml\" "
      "ContentType=\"application/"
      "vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>";
  for (int i = 0; i < sheets.size(); ++i) {
    types +=
        "<Override PartName=\"/xl/worksheets/sheet" +
        QByteArray::number(i + 1) +
        ".xml\" "
        "ContentType=\"application/"
        "vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
  }
  types += "</Types>";
  zip.addFile("[Content_Types].xml", types);
  zip.addFile("_rels/.rels",
              "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships "
              "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
              "relationships\"><Relationship Id=\"rId1\" "
              "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
              "relationships/officeDocument\" "
              "Target=\"xl/workbook.xml\"/></Relationships>");
  QByteArray
      wb =
          "<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook "
          "xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
          "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/"
          "relationships\"><sheets>",
      rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships "
             "xmlns=\"http://schemas.openxmlformats.org/package/2006/"
             "relationships\">";
  for (int i = 0; i < sheets.size(); ++i) {
    wb += "<sheet name=\"" + sheets[i].first.toHtmlEscaped().toUtf8() +
          "\" sheetId=\"" + QByteArray::number(i + 1) + "\" r:id=\"rId" +
          QByteArray::number(i + 1) + "\"/>";
    rels += "<Relationship Id=\"rId" + QByteArray::number(i + 1) +
            "\" "
            "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
            "relationships/worksheet\" Target=\"worksheets/sheet" +
            QByteArray::number(i + 1) + ".xml\"/>";
    zip.addFile("xl/worksheets/sheet" + QByteArray::number(i + 1) + ".xml",
                sheetXml(sheets[i].second));
  }
  wb += "</sheets></workbook>";
  rels += "</Relationships>";
  zip.addFile("xl/workbook.xml", wb);
  zip.addFile("xl/_rels/workbook.xml.rels", rels);
  zip.close();
  if (zip.status() != QZipWriter::NoError || QFileInfo(path).size() == 0) {
    if (error)
      *error = "Échec de finalisation du classeur.";
    return false;
  }
  return true;
#endif
}
QString json(const InterfaceRecord &r) {
  QJsonObject o{{"code", r.code},
                {"element1", r.element1Id},
                {"element2", r.element2Id},
                {"status", r.status},
                {"description", r.description},
                {"archived", r.archived}};
  return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}
} // namespace

InterfaceService::InterfaceService(QString name)
    : m_connectionName(std::move(name)) {}
void InterfaceService::setConnectionName(const QString &name) {
  m_connectionName = name;
}

QList<InterfaceRecord> InterfaceService::find(const InterfaceFilter &f,
                                              QString *error) const {
  QList<InterfaceRecord> out;
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen()) {
    if (error)
      *error = "La base de données n'est pas ouverte.";
    return out;
  }
  QString sql =
      "SELECT "
      "I.ID,I.ELEMENT1,I.ELEMENT2,COALESCE(I.CODE,'IF-'||I.ID),COALESCE(I."
      "DESCRIPTION,''),COALESCE(I.STATUS,'DRAFT'),COALESCE(I.ARCHIVED,0),P1."
      "NAME,P2.NAME,COALESCE((SELECT GROUP_CONCAT(T.LABEL, ', ') FROM "
      "INTERFACE_TYPE_LINK L JOIN INTERFACE_TYPE T ON T.ID=L.TYPE_ID WHERE "
      "L.INTERFACE_ID=I.ID),''),COALESCE((SELECT GROUP_CONCAT(R.CODE, ', ') "
      "FROM INTERFACE_REQUIREMENT L JOIN REQUIREMENT R ON R.ID=L.REQ_ID WHERE "
      "L.INTERFACE_ID=I.ID),''),COALESCE((SELECT "
      "GROUP_CONCAT(COALESCE(NULLIF(D.REFERENCE,''),D.TITLE), ', ') FROM "
      "INTERFACE_DOCUMENT L JOIN DOCUMENT D ON D.ID=L.DOC_ID WHERE "
      "L.INTERFACE_ID=I.ID),'') FROM INTERFACE I JOIN PT P1 ON "
      "P1.ID=I.ELEMENT1 JOIN PT P2 ON P2.ID=I.ELEMENT2 WHERE 1=1";
  QVariantList b;
  if (!f.includeArchived)
    sql += " AND COALESCE(I.ARCHIVED,0)=0";
  if (f.ptId >= 0) {
    sql += " AND (I.ELEMENT1=? OR I.ELEMENT2=?)";
    b << f.ptId << f.ptId;
  }
  if (f.typeId >= 0) {
    sql += " AND EXISTS(SELECT 1 FROM INTERFACE_TYPE_LINK X WHERE "
           "X.INTERFACE_ID=I.ID AND X.TYPE_ID=?)";
    b << f.typeId;
  }
  if (f.onlyWithoutIcd)
    sql += " AND NOT EXISTS(SELECT 1 FROM INTERFACE_DOCUMENT X WHERE "
           "X.INTERFACE_ID=I.ID)";
  if (!f.text.trimmed().isEmpty()) {
    sql += " AND (I.CODE LIKE ? OR I.DESCRIPTION LIKE ? OR P1.NAME LIKE ? OR "
           "P2.NAME LIKE ?)";
    QString s = "%" + f.text.trimmed() + "%";
    b << s << s << s << s;
  }
  sql += " ORDER BY I.ARCHIVED,I.CODE,I.ID";
  QSqlQuery q(db);
  q.prepare(sql);
  for (const auto &v : b)
    q.addBindValue(v);
  if (!q.exec()) {
    if (error)
      *error = q.lastError().text();
    return out;
  }
  while (q.next()) {
    InterfaceRecord r;
    r.id = q.value(0).toInt();
    r.element1Id = q.value(1).toInt();
    r.element2Id = q.value(2).toInt();
    r.code = q.value(3).toString();
    r.description = q.value(4).toString();
    r.status = q.value(5).toString();
    r.archived = q.value(6).toBool();
    r.element1 = q.value(7).toString();
    r.element2 = q.value(8).toString();
    r.types = q.value(9).toString();
    r.requirements = q.value(10).toString();
    r.documents = q.value(11).toString();
    out << r;
  }
  return out;
}
InterfaceRecord InterfaceService::get(int id, QString *error) const {
  InterfaceFilter f;
  f.includeArchived = true;
  auto rows = find(f, error);
  for (auto &r : rows)
    if (r.id == id) {
      QSqlDatabase db = database(m_connectionName);
      QSqlQuery q(db);
      q.prepare("SELECT TYPE_ID FROM INTERFACE_TYPE_LINK WHERE INTERFACE_ID=? "
                "ORDER BY TYPE_ID");
      q.addBindValue(id);
      q.exec();
      while (q.next())
        r.typeIds << q.value(0).toInt();
      q.prepare("SELECT REQ_ID FROM INTERFACE_REQUIREMENT WHERE INTERFACE_ID=? "
                "ORDER BY REQ_ID");
      q.addBindValue(id);
      q.exec();
      while (q.next())
        r.requirementIds << q.value(0).toInt();
      q.prepare("SELECT DOC_ID,CHAPTER_NODE_ID FROM INTERFACE_DOCUMENT WHERE "
                "INTERFACE_ID=? ORDER BY DOC_ID");
      q.addBindValue(id);
      q.exec();
      while (q.next()) {
        int doc = q.value(0).toInt();
        r.documentIds << doc;
        if (!q.value(1).isNull())
          r.documentChapters.insert(doc, q.value(1).toInt());
      }
      return r;
    }
  if (error && error->isEmpty())
    *error = "Interface introuvable.";
  return {};
}

RequirementResult InterfaceService::save(const InterfaceRecord &r) {
  if (r.element1Id < 0 || r.element2Id < 0 || r.element1Id == r.element2Id)
    return RequirementResult::failure(
        "Sélectionnez deux éléments PT distincts.");
  if (r.code.trimmed().isEmpty())
    return RequirementResult::failure("Le code est obligatoire.");
  QSqlDatabase db = database(m_connectionName);
  if (!db.isValid() || !db.isOpen())
    return RequirementResult::failure("La base de données n'est pas ouverte.");
  InterfaceRecord before;
  if (r.id >= 0)
    before = get(r.id);
  if (!db.transaction())
    return RequirementResult::failure(db.lastError().text());
  QSqlQuery q(db);
  int id = r.id;
  if (id < 0) {
    q.prepare("INSERT INTO "
              "INTERFACE(ELEMENT1,ELEMENT2,CODE,DESCRIPTION,STATUS,ARCHIVED,"
              "CREATED_AT,UPDATED_AT) "
              "VALUES(?,?,?,?,?,0,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");
  } else {
    q.prepare("UPDATE INTERFACE SET "
              "ELEMENT1=?,ELEMENT2=?,CODE=?,DESCRIPTION=?,STATUS=?,UPDATED_AT="
              "CURRENT_TIMESTAMP WHERE ID=?");
  }
  q.addBindValue(r.element1Id);
  q.addBindValue(r.element2Id);
  q.addBindValue(r.code.trimmed());
  q.addBindValue(r.description);
  q.addBindValue(r.status.isEmpty() ? "DRAFT" : r.status);
  if (id >= 0)
    q.addBindValue(id);
  if (!q.exec()) {
    db.rollback();
    return RequirementResult::failure(q.lastError().text());
  }
  if (id < 0)
    id = q.lastInsertId().toInt();
  for (const QString t :
       {"INTERFACE_TYPE_LINK", "INTERFACE_REQUIREMENT", "INTERFACE_DOCUMENT"}) {
    QSqlQuery d(db);
    d.prepare("DELETE FROM " + t + " WHERE INTERFACE_ID=?");
    d.addBindValue(id);
    if (!d.exec()) {
      db.rollback();
      return RequirementResult::failure(d.lastError().text());
    }
  }
  auto links = [&](const QString &sql, const QList<int> &ids) {
    for (int v : ids) {
      QSqlQuery i(db);
      i.prepare(sql);
      i.addBindValue(id);
      i.addBindValue(v);
      if (!i.exec())
        return i.lastError().text();
    }
    return QString();
  };
  QString e =
      links("INSERT INTO INTERFACE_TYPE_LINK(INTERFACE_ID,TYPE_ID) VALUES(?,?)",
            r.typeIds);
  if (e.isEmpty())
    e = links(
        "INSERT INTO INTERFACE_REQUIREMENT(INTERFACE_ID,REQ_ID) VALUES(?,?)",
        r.requirementIds);
  if (e.isEmpty())
    for (int doc : r.documentIds) {
      QSqlQuery i(db);
      i.prepare(
          "INSERT INTO INTERFACE_DOCUMENT(INTERFACE_ID,DOC_ID,CHAPTER_NODE_ID) "
          "VALUES(?,?,?)");
      i.addBindValue(id);
      i.addBindValue(doc);
      i.addBindValue(r.documentChapters.contains(doc)
                         ? QVariant(r.documentChapters.value(doc))
                         : QVariant());
      if (!i.exec()) {
        e = i.lastError().text();
        break;
      }
    }
  if (!e.isEmpty()) {
    db.rollback();
    return RequirementResult::failure(e);
  }
  QSqlQuery log(db);
  log.prepare("INSERT INTO "
              "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_"
              "JSON) VALUES(?,?,?,?,?)");
  log.addBindValue(r.id < 0 ? "CREATE" : "UPDATE");
  log.addBindValue("INTERFACE");
  log.addBindValue(id);
  log.addBindValue(r.id < 0 ? QVariant() : QVariant(json(before)));
  InterfaceRecord after = r;
  after.id = id;
  log.addBindValue(json(after));
  if (!log.exec() || !db.commit()) {
    db.rollback();
    return RequirementResult::failure(log.lastError().text().isEmpty()
                                          ? db.lastError().text()
                                          : log.lastError().text());
  }
  return RequirementResult::successResult("Interface enregistrée.", id, r.code);
}
RequirementResult InterfaceService::setArchived(int id, bool archived) {
  QSqlDatabase db = database(m_connectionName);
  QSqlQuery q(db);
  q.prepare("UPDATE INTERFACE SET ARCHIVED=?,UPDATED_AT=CURRENT_TIMESTAMP "
            "WHERE ID=?");
  q.addBindValue(archived);
  q.addBindValue(id);
  if (!q.exec() || q.numRowsAffected() != 1)
    return RequirementResult::failure(q.lastError().text().isEmpty()
                                          ? "Interface introuvable."
                                          : q.lastError().text());
  QSqlQuery l(db);
  l.prepare(
      "INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
      "VALUES(?,?,?,?)");
  l.addBindValue(archived ? "ARCHIVE" : "RESTORE");
  l.addBindValue("INTERFACE");
  l.addBindValue(id);
  l.addBindValue(QString("{\"archived\":%1}").arg(archived ? "true" : "false"));
  l.exec();
  return RequirementResult::successResult(
      archived ? "Interface archivée." : "Interface restaurée.", id);
}
RequirementResult InterfaceService::saveType(int id, const QString &code,
                                             const QString &label) {
  if (code.trimmed().isEmpty() || label.trimmed().isEmpty())
    return RequirementResult::failure("Code et libellé sont obligatoires.");
  QSqlQuery q(database(m_connectionName));
  if (id < 0)
    q.prepare("INSERT INTO INTERFACE_TYPE(CODE,LABEL) VALUES(UPPER(?),?)");
  else {
    q.prepare("UPDATE INTERFACE_TYPE SET CODE=UPPER(?),LABEL=? WHERE ID=?");
    q.addBindValue(code.trimmed());
    q.addBindValue(label.trimmed());
    q.addBindValue(id);
    if (!q.exec())
      return RequirementResult::failure(q.lastError().text());
    return RequirementResult::successResult("Type enregistré.", id);
  }
  q.addBindValue(code.trimmed());
  q.addBindValue(label.trimmed());
  if (!q.exec())
    return RequirementResult::failure(q.lastError().text());
  return RequirementResult::successResult("Type enregistré.",
                                          q.lastInsertId().toInt());
}
RequirementResult InterfaceService::removeType(int id) {
  QSqlDatabase db = database(m_connectionName);
  QSqlQuery c(db);
  c.prepare("SELECT COUNT(*) FROM INTERFACE_TYPE_LINK WHERE TYPE_ID=?");
  c.addBindValue(id);
  c.exec();
  c.next();
  if (c.value(0).toInt())
    return RequirementResult::failure("Ce type est utilisé par une interface.");
  QSqlQuery q(db);
  q.prepare("DELETE FROM INTERFACE_TYPE WHERE ID=?");
  q.addBindValue(id);
  if (!q.exec())
    return RequirementResult::failure(q.lastError().text());
  return RequirementResult::successResult("Type supprimé.", id);
}
QList<N2Cell> InterfaceService::matrix(bool archived, QString *error) const {
  QList<N2Cell> out;
  QSqlQuery q(database(m_connectionName));
  QString sql =
      "SELECT "
      "MIN(I.ELEMENT1,I.ELEMENT2),MAX(I.ELEMENT1,I.ELEMENT2),COUNT(DISTINCT "
      "I.ID),COUNT(DISTINCT CASE WHEN D.DOC_ID IS NOT NULL THEN I.ID "
      "END),COUNT(DISTINCT L.TYPE_ID),COALESCE(GROUP_CONCAT(DISTINCT "
      "T.LABEL),'') FROM INTERFACE I LEFT JOIN INTERFACE_DOCUMENT D ON "
      "D.INTERFACE_ID=I.ID LEFT JOIN INTERFACE_TYPE_LINK L ON "
      "L.INTERFACE_ID=I.ID LEFT JOIN INTERFACE_TYPE T ON T.ID=L.TYPE_ID";
  if (!archived)
    sql += " WHERE COALESCE(I.ARCHIVED,0)=0";
  sql += " GROUP BY MIN(I.ELEMENT1,I.ELEMENT2),MAX(I.ELEMENT1,I.ELEMENT2)";
  if (!q.exec(sql)) {
    if (error)
      *error = q.lastError().text();
    return out;
  }
  while (q.next()) {
    N2Cell c;
    c.firstPtId = q.value(0).toInt();
    c.secondPtId = q.value(1).toInt();
    c.interfaceCount = q.value(2).toInt();
    c.coveredCount = q.value(3).toInt();
    c.typeCount = q.value(4).toInt();
    c.label = q.value(5).toString();
    out << c;
  }
  return out;
}
RequirementResult InterfaceService::exportXlsx(const QString &path,
                                               const InterfaceFilter &f) const {
  QString error;
  auto rows = find(f, &error);
  if (!error.isEmpty())
    return RequirementResult::failure(error);
  QSqlDatabase db = database(m_connectionName);
  QList<int> pts;
  QStringList names;
  QSqlQuery p("SELECT ID,NAME FROM PT WHERE COALESCE(ARCHIVED,0)=0 ORDER BY "
              "POSITION,ID",
              db);
  while (p.next()) {
    pts << p.value(0).toInt();
    names << p.value(1).toString();
  }
  auto cells = matrix(false, &error);
  QList<QStringList> n2;
  QStringList head{"Product Tree"};
  head << names;
  n2 << head;
  int covered = 0, total = 0;
  for (int a = 0; a < pts.size(); ++a) {
    QStringList line{names[a]};
    for (int b = 0; b < pts.size(); ++b) {
      QString v = a == b ? "—" : "";
      for (const auto &c : cells)
        if (qMin(pts[a], pts[b]) == c.firstPtId &&
            qMax(pts[a], pts[b]) == c.secondPtId) {
          v = QString("%1 (%2/%3 ICD)")
                  .arg(c.label.isEmpty() ? QString::number(c.interfaceCount)
                                         : c.label)
                  .arg(c.coveredCount)
                  .arg(c.interfaceCount);
          if (a < b) {
            total += c.interfaceCount;
            covered += c.coveredCount;
          }
          break;
        }
      line << v;
    }
    n2 << line;
  }
  QList<QStringList> details{{"Code", "Élément 1", "Élément 2", "Statut",
                              "Types", "Exigences", "ICD", "Description",
                              "Archivée"}};
  for (const auto &r : rows)
    details << QStringList{
        r.code,      r.element1,    r.element2,
        r.status,    r.types,       r.requirements,
        r.documents, r.description, r.archived ? "Oui" : "Non"};
  QList<QStringList> summary{
      {"Indicateur", "Valeur"},
      {"Interfaces", QString::number(total)},
      {"Couvertes par ICD", QString::number(covered)},
      {"Non couvertes", QString::number(total - covered)},
      {"Taux de couverture ICD",
       total ? QString::number(100. * covered / total, 'f', 2) + " %"
             : "0,00 %"}};
  if (!workbook(
          path,
          {{"Synthèse", summary}, {"Matrice N2", n2}, {"Interfaces", details}},
          &error))
    return RequirementResult::failure(error);
  return RequirementResult::successResult("Interfaces et matrice N² exportées.",
                                          -1, path);
}
