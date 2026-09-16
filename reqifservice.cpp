#include "reqifservice.h"

#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <functional>

namespace {
QString refText(QXmlStreamReader &xml) {
  return xml.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
}

QString displayName(const QMap<QString, QString> &names, const QString &id) {
  return names.value(id, id);
}

int scalarId(QSqlDatabase db, const QString &sql) {
  QSqlQuery q(db);
  return q.exec(sql) && q.next() ? q.value(0).toInt() : -1;
}

QString safeCode(QString code, const QString &externalId) {
  code = code.trimmed();
  if (!code.isEmpty()) return code;
  QString result = externalId;
  result.replace(QRegularExpression("[^A-Za-z0-9_.-]"), "-");
  return result.isEmpty() ? QStringLiteral("REQIF-OBJECT") : result;
}
}

QString ReqIfReport::summary() const {
  return QObject::tr("%1 exigences (%2 créations, %3 mises à jour), %4 relations, "
                     "%5 avertissements, %6 erreurs")
      .arg(objects.size()).arg(creates).arg(updates).arg(relations.size())
      .arg(warnings.size()).arg(errors.size());
}

ReqIfService::ReqIfService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

ReqIfReport ReqIfService::preview(const QString &fileName) const {
  ReqIfReport report;
  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly)) {
    report.errors << file.errorString();
    return report;
  }
  const QByteArray head = file.peek(65536).toLower();
  if (head.contains("<!doctype") || head.contains("<!entity")) {
    report.errors << QObject::tr("DTD et entités XML sont interdites dans un fichier ReqIF.");
    return report;
  }

  QXmlStreamReader xml(&file);
  QMap<QString, QString> definitionNames, enumNames;
  QMap<QString, ReqIfObject> objects;
  QMap<QString, QString> hierarchyObject, hierarchyParent;
  QStringList hierarchyStack;
  QString currentObject, currentRelation;
  ReqIfRelation relation;
  struct PendingValue { QString definition, value, type; } pending;

  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isEndElement()) {
      const QString end = xml.name().toString();
      if (end == "SPEC-OBJECT") currentObject.clear();
      else if (end == "SPEC-RELATION") { report.relations << relation; currentRelation.clear(); }
      else if (end == "SPEC-HIERARCHY" && !hierarchyStack.isEmpty()) hierarchyStack.removeLast();
      continue;
    }
    if (!xml.isStartElement()) continue;
    const QString n = xml.name().toString();
    const auto a = xml.attributes();
    const QString id = a.value("IDENTIFIER").toString();
    const QString longName = a.value("LONG-NAME").toString();
    if ((n.startsWith("ATTRIBUTE-DEFINITION-") || n == "SPEC-OBJECT-TYPE" ||
         n == "SPEC-RELATION-TYPE") && !id.isEmpty())
      definitionNames[id] = longName.isEmpty() ? id : longName;
    if (n == "ENUM-VALUE" && !id.isEmpty())
      enumNames[id] = longName.isEmpty() ? id : longName;
    if (n == "SPEC-OBJECT") {
      if (objects.contains(id))
        report.errors << QObject::tr("IDENTIFIER ReqIF dupliqué : %1").arg(id);
      currentObject = id;
      ReqIfObject o; o.externalId = id; o.title = longName;
      objects[id] = o;
    } else if (n == "SPEC-RELATION") {
      currentRelation = id; relation = {}; relation.externalId = id;
    } else if (n == "SPEC-HIERARCHY") {
      if (!hierarchyStack.isEmpty()) hierarchyParent[id] = hierarchyStack.last();
      hierarchyStack << id;
    } else if (n == "SPEC-OBJECT-REF") {
      const QString ref = refText(xml);
      if (!hierarchyStack.isEmpty()) hierarchyObject[hierarchyStack.last()] = ref;
    } else if (!currentRelation.isEmpty() && n == "SOURCE") {
      while (xml.readNextStartElement())
        if (xml.name() == u"SPEC-OBJECT-REF") relation.sourceExternalId = refText(xml); else xml.skipCurrentElement();
    } else if (!currentRelation.isEmpty() && n == "TARGET") {
      while (xml.readNextStartElement())
        if (xml.name() == u"SPEC-OBJECT-REF") relation.targetExternalId = refText(xml); else xml.skipCurrentElement();
    } else if (!currentRelation.isEmpty() && n == "SPEC-RELATION-TYPE-REF") {
      relation.type = displayName(definitionNames, refText(xml));
    } else if (!currentObject.isEmpty() && n.startsWith("ATTRIBUTE-VALUE-")) {
      pending = {}; pending.type = n.mid(QString("ATTRIBUTE-VALUE-").size());
      pending.value = a.value("THE-VALUE").toString();
      if (pending.type == "XHTML")
        report.warnings << QObject::tr("%1 contient du XHTML : ce contenu riche n'est pas pris en charge et doit être contrôlé avant import.").arg(currentObject);
    } else if (!currentObject.isEmpty() && n == "ENUM-VALUE-REF") {
      const QString enumId=refText(xml);pending.value=enumNames.value(enumId,enumId);
    } else if (!currentObject.isEmpty() && n.startsWith("ATTRIBUTE-DEFINITION-") && n.endsWith("-REF")) {
      pending.definition = refText(xml);
      ReqIfAttribute value{pending.definition, displayName(definitionNames, pending.definition), pending.value, pending.type};
      objects[currentObject].attributes << value;
      const QString key = value.name.toLower();
      if (key.contains("code") || key.contains("identifiant")) objects[currentObject].code = value.value;
      else if (key.contains("title") || key.contains("titre") || key == "reqif.name") objects[currentObject].title = value.value;
      else if (key.contains("description") || key.contains("text")) objects[currentObject].description = value.value;
      else if (key.contains("status") || key.contains("statut")) objects[currentObject].status = value.value;
      else if (key.contains("type")) objects[currentObject].type = value.value;
      else if (key.contains("product tree") || key == "pt") objects[currentObject].productTrees = value.value.split(';', Qt::SkipEmptyParts);
    }
  }
  if (xml.hasError()) report.errors << QObject::tr("XML invalide : %1 (ligne %2)").arg(xml.errorString()).arg(xml.lineNumber());
  if (objects.isEmpty() && report.errors.isEmpty()) report.errors << QObject::tr("Aucun SPEC-OBJECT trouvé.");
  QSet<QString> codes;
  for (ReqIfObject o : objects) {
    if (o.externalId.isEmpty()) { report.errors << QObject::tr("SPEC-OBJECT sans IDENTIFIER."); continue; }
    o.code = safeCode(o.code, o.externalId);
    if (o.title.isEmpty()) o.title = o.code;
    if (codes.contains(o.code)) { ++report.duplicates; report.errors << QObject::tr("Code dupliqué dans le fichier : %1").arg(o.code); }
    codes.insert(o.code);
    const QString hierarchyId = hierarchyObject.key(o.externalId);
    o.parentExternalId = hierarchyObject.value(hierarchyParent.value(hierarchyId));
    report.objects << o;
  }
  QSet<QString> ids = QSet<QString>(objects.keyBegin(), objects.keyEnd());
  for (const QString &ref : hierarchyObject)
    if (!ids.contains(ref)) report.errors << QObject::tr("Hiérarchie : référence cassée vers %1.").arg(ref);
  for (const ReqIfRelation &r : report.relations)
    if (!ids.contains(r.sourceExternalId) || !ids.contains(r.targetExternalId))
      report.errors << QObject::tr("Relation %1 : référence source ou cible cassée.").arg(r.externalId);
  QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
  if (db.isValid() && db.isOpen()) for (const ReqIfObject &o : report.objects) {
    QSqlQuery q(db); q.prepare("SELECT 1 FROM REQIF_IDENTITY WHERE EXTERNAL_ID=?"); q.addBindValue(o.externalId);
    if (q.exec() && q.next()) ++report.updates; else ++report.creates;
    QSqlQuery conflict(db); conflict.prepare("SELECT R.ID FROM REQUIREMENT R LEFT JOIN REQIF_IDENTITY I ON I.REQ_ID=R.ID WHERE R.CODE=? AND COALESCE(I.EXTERNAL_ID,'')<>?"); conflict.addBindValue(o.code); conflict.addBindValue(o.externalId);
    if (conflict.exec() && conflict.next()) report.errors << QObject::tr("Le code %1 appartient déjà à une autre exigence.").arg(o.code);
  }
  QMap<QString, QString> externalParents;
  for (const ReqIfObject &o : report.objects) externalParents[o.externalId]=o.parentExternalId;
  for (const ReqIfObject &o : report.objects) {
    QSet<QString> visited; QString parent=o.parentExternalId;
    while (!parent.isEmpty()) { if (parent==o.externalId || visited.contains(parent)) { report.errors << QObject::tr("Cycle hiérarchique détecté autour de %1.").arg(o.externalId); break; } visited.insert(parent); parent=externalParents.value(parent); }
  }
  return report;
}

bool ReqIfService::importFile(const QString &fileName, const ReqIfReport &report, QString *error) const {
  if (!report.valid()) { if (error) *error = report.errors.join('\n'); return false; }
  QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
  if (!db.isValid() || !db.isOpen() || !db.transaction()) { if (error) *error = db.lastError().text(); return false; }
  auto fail = [&](const QString &message) { db.rollback(); if (error) *error = message; return false; };
  const int defaultPt = scalarId(db, "SELECT ID FROM PT ORDER BY ID LIMIT 1");
  const int defaultType = scalarId(db, "SELECT ID FROM REQ_TYPE ORDER BY ID LIMIT 1");
  const int defaultStatus = scalarId(db, "SELECT ID FROM REQ_STATUS ORDER BY ID LIMIT 1");
  const int defaultMethod = scalarId(db, "SELECT ID FROM REQ_METHOD ORDER BY ID LIMIT 1");
  int defaultDoc = scalarId(db, "SELECT ID FROM DOCUMENT ORDER BY ID LIMIT 1");
  if (defaultPt < 0 || defaultType < 0 || defaultStatus < 0 || defaultMethod < 0)
    return fail(QObject::tr("Les catalogues PT, type, statut et méthode doivent contenir une valeur."));
  if (defaultDoc < 0) { QSqlQuery q(db); q.prepare("INSERT INTO DOCUMENT(PT_ID,TYPE,TITLE,DESCRIPTION) VALUES(?,COALESCE((SELECT ID FROM DOC_TYPE ORDER BY ID LIMIT 1),1),?,?)"); q.addBindValue(defaultPt); q.addBindValue(QObject::tr("Import ReqIF")); q.addBindValue(QObject::tr("Conteneur créé pour l'import ReqIF.")); if (!q.exec()) return fail(q.lastError().text()); defaultDoc=q.lastInsertId().toInt(); }
  QMap<QString,int> localIds;
  for (const ReqIfObject &o : report.objects) {
    QSqlQuery existing(db); existing.prepare("SELECT REQ_ID FROM REQIF_IDENTITY WHERE EXTERNAL_ID=?"); existing.addBindValue(o.externalId);
    int reqId=-1; if (!existing.exec()) return fail(existing.lastError().text()); if (existing.next()) reqId=existing.value(0).toInt();
    int typeId=defaultType,statusId=defaultStatus;
    if (!o.type.isEmpty()) { QSqlQuery q(db);q.prepare("SELECT ID FROM REQ_TYPE WHERE UPPER(TYPE)=UPPER(?)");q.addBindValue(o.type);if(q.exec()&&q.next())typeId=q.value(0).toInt(); }
    if (!o.status.isEmpty()) { QSqlQuery q(db);q.prepare("SELECT ID FROM REQ_STATUS WHERE UPPER(STATUS)=UPPER(?) OR UPPER(SHORTCUT)=UPPER(?)");q.addBindValue(o.status);q.addBindValue(o.status);if(q.exec()&&q.next())statusId=q.value(0).toInt(); }
    QSqlQuery q(db);
    if (reqId < 0) { q.prepare("INSERT INTO REQUIREMENT(PT_ID,CODE,DOC_ID,TITLE,DESCRIPTION,TYPE,STATUS,SOURCE,VERIF_METHOD) VALUES(?,?,?,?,?,?,?, ?,?)"); q.addBindValue(defaultPt);q.addBindValue(o.code);q.addBindValue(defaultDoc);q.addBindValue(o.title);q.addBindValue(o.description);q.addBindValue(typeId);q.addBindValue(statusId);q.addBindValue(QString("ReqIF:%1").arg(QFileInfo(fileName).fileName()));q.addBindValue(defaultMethod); }
    else { q.prepare("UPDATE REQUIREMENT SET CODE=?,TITLE=?,DESCRIPTION=?,TYPE=?,STATUS=? WHERE ID=?");q.addBindValue(o.code);q.addBindValue(o.title);q.addBindValue(o.description);q.addBindValue(typeId);q.addBindValue(statusId);q.addBindValue(reqId); }
    if (!q.exec()) return fail(q.lastError().text());
    if (reqId < 0) reqId = q.lastInsertId().toInt();
    localIds[o.externalId] = reqId;
    QSqlQuery identity(db);identity.prepare("INSERT INTO REQIF_IDENTITY(REQ_ID,EXTERNAL_ID,SOURCE_DOCUMENT,LAST_SYNC_AT) VALUES(?,?,?,CURRENT_TIMESTAMP) ON CONFLICT(REQ_ID) DO UPDATE SET EXTERNAL_ID=excluded.EXTERNAL_ID,SOURCE_DOCUMENT=excluded.SOURCE_DOCUMENT,LAST_SYNC_AT=CURRENT_TIMESTAMP");identity.addBindValue(reqId);identity.addBindValue(o.externalId);identity.addBindValue(QFileInfo(fileName).fileName());if(!identity.exec())return fail(identity.lastError().text());
    for(const ReqIfAttribute&a:o.attributes){QSqlQuery aq(db);aq.prepare("INSERT INTO REQIF_ATTRIBUTE(REQ_ID,ATTRIBUTE_ID,ATTRIBUTE_NAME,VALUE,VALUE_TYPE) VALUES(?,?,?,?,?) ON CONFLICT(REQ_ID,ATTRIBUTE_ID) DO UPDATE SET ATTRIBUTE_NAME=excluded.ATTRIBUTE_NAME,VALUE=excluded.VALUE,VALUE_TYPE=excluded.VALUE_TYPE");aq.addBindValue(reqId);aq.addBindValue(a.id);aq.addBindValue(a.name);aq.addBindValue(a.value);aq.addBindValue(a.type);if(!aq.exec())return fail(aq.lastError().text());}
    for (QString ptName : o.productTrees) { ptName=ptName.trimmed(); QSqlQuery pt(db); pt.prepare("SELECT ID FROM PT WHERE UPPER(NAME)=UPPER(?) OR UPPER(SEGMENT)=UPPER(?)");pt.addBindValue(ptName);pt.addBindValue(ptName);if(pt.exec()&&pt.next()){QSqlQuery alloc(db);alloc.prepare("INSERT OR IGNORE INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(?,?,0)");alloc.addBindValue(reqId);alloc.addBindValue(pt.value(0));if(!alloc.exec())return fail(alloc.lastError().text());} }
  }
  for (const ReqIfObject &o : report.objects) if (!o.parentExternalId.isEmpty()) { const int child=localIds.value(o.externalId,-1),parent=localIds.value(o.parentExternalId,-1);if(child<0||parent<0)return fail(QObject::tr("Parent hiérarchique non résolu pour %1.").arg(o.externalId));QSqlQuery p(db);p.prepare("UPDATE REQUIREMENT SET PARENT_ID=? WHERE ID=?");p.addBindValue(parent);p.addBindValue(child);if(!p.exec())return fail(p.lastError().text());QSqlQuery link(db);link.prepare("INSERT OR IGNORE INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID,COMMENT) VALUES(?,?,1,'Hiérarchie ReqIF')");link.addBindValue(parent);link.addBindValue(child);if(!link.exec())return fail(link.lastError().text());}
  for(const ReqIfRelation&r:report.relations){const int s=localIds.value(r.sourceExternalId,-1),t=localIds.value(r.targetExternalId,-1);if(s<0||t<0)return fail(QObject::tr("Référence de relation non résolue."));int typeId=3;QSqlQuery tq(db);tq.prepare("SELECT ID FROM REQUIREMENT_RELATION_TYPE WHERE UPPER(CODE)=UPPER(?) OR UPPER(LABEL)=UPPER(?)");tq.addBindValue(r.type);tq.addBindValue(r.type);if(tq.exec()&&tq.next())typeId=tq.value(0).toInt();QSqlQuery q(db);q.prepare("INSERT OR IGNORE INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID,COMMENT) VALUES(?,?,?,?)");q.addBindValue(s);q.addBindValue(t);q.addBindValue(typeId);q.addBindValue(QString("ReqIF:%1").arg(r.externalId));if(!q.exec())return fail(q.lastError().text());}
  { QSqlQuery audit(db); audit.prepare("INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,AFTER_JSON,COMMENT) VALUES('IMPORT','REQIF',json_object('file',?,'objects',?,'relations',?),?)"); audit.addBindValue(QFileInfo(fileName).fileName()); audit.addBindValue(report.objects.size()); audit.addBindValue(report.relations.size()); audit.addBindValue(QObject::tr("Import ReqIF groupé")); if(!audit.exec())return fail(audit.lastError().text()); }
  if (!db.commit()) return fail(db.lastError().text());
  return true;
}

bool ReqIfService::exportFile(const QString &fileName, QString *error) const {
  QSqlDatabase db=QSqlDatabase::database(m_connectionName,false);QSaveFile file(fileName);if(!file.open(QIODevice::WriteOnly)){if(error)*error=file.errorString();return false;}QXmlStreamWriter x(&file);x.setAutoFormatting(true);x.writeStartDocument();x.writeStartElement("REQ-IF");x.writeDefaultNamespace("http://www.omg.org/spec/ReqIF/20110401/reqif.xsd");
  x.writeStartElement("THE-HEADER");x.writeStartElement("REQ-IF-HEADER");x.writeAttribute("IDENTIFIER","GESIER-HEADER");x.writeTextElement("REQ-IF-TOOL-ID","GeSiER");x.writeTextElement("REQ-IF-VERSION","1.0");x.writeEndElement();x.writeEndElement();
  x.writeStartElement("CORE-CONTENT");x.writeStartElement("REQ-IF-CONTENT");x.writeEmptyElement("DATATYPES");x.writeStartElement("SPEC-TYPES");x.writeStartElement("SPEC-OBJECT-TYPE");x.writeAttribute("IDENTIFIER","GESIER-REQUIREMENT-TYPE");x.writeStartElement("SPEC-ATTRIBUTES");auto definition=[&](const QString&id,const QString&name){x.writeEmptyElement("ATTRIBUTE-DEFINITION-STRING");x.writeAttribute("IDENTIFIER",id);x.writeAttribute("LONG-NAME",name);};definition("GESIER-CODE","Code");definition("GESIER-DESCRIPTION","Description");definition("GESIER-TYPE","Type");definition("GESIER-STATUS","Status");definition("GESIER-PT","Product Tree");QSqlQuery definitions(db);if(definitions.exec("SELECT DISTINCT ATTRIBUTE_ID,ATTRIBUTE_NAME FROM REQIF_ATTRIBUTE WHERE ATTRIBUTE_ID NOT LIKE 'GESIER-%' ORDER BY ATTRIBUTE_ID"))while(definitions.next())definition(definitions.value(0).toString(),definitions.value(1).toString());x.writeEndElement();x.writeEndElement();x.writeEmptyElement("SPEC-RELATION-TYPE");x.writeAttribute("IDENTIFIER","GESIER-RELATION-TYPE");x.writeEmptyElement("SPECIFICATION-TYPE");x.writeAttribute("IDENTIFIER","GESIER-SPECIFICATION-TYPE");x.writeEndElement();
  x.writeStartElement("SPEC-OBJECTS");QSqlQuery q(db);if(!q.exec("SELECT R.ID,R.CODE,R.TITLE,R.DESCRIPTION,COALESCE(I.EXTERNAL_ID,'GESIER-'||R.ID),R.PARENT_ID,T.TYPE,S.STATUS,COALESCE((SELECT GROUP_CONCAT(P.NAME,';') FROM REQUIREMENT_PT RP JOIN PT P ON P.ID=RP.PT_ID WHERE RP.REQ_ID=R.ID),'') FROM REQUIREMENT R LEFT JOIN REQIF_IDENTITY I ON I.REQ_ID=R.ID LEFT JOIN REQ_TYPE T ON T.ID=R.TYPE LEFT JOIN REQ_STATUS S ON S.ID=R.STATUS ORDER BY R.CODE")){if(error)*error=q.lastError().text();return false;}QMap<int,QString> ids;QMap<int,int> parents;while(q.next()){const int localId=q.value(0).toInt();ids[localId]=q.value(4).toString();parents[localId]=q.value(5).isNull()?-1:q.value(5).toInt();x.writeStartElement("SPEC-OBJECT");x.writeAttribute("IDENTIFIER",q.value(4).toString());x.writeAttribute("LONG-NAME",q.value(2).toString());x.writeStartElement("VALUES");auto value=[&](const QString&id,const QString&v){x.writeStartElement("ATTRIBUTE-VALUE-STRING");x.writeAttribute("THE-VALUE",v);x.writeStartElement("DEFINITION");x.writeTextElement("ATTRIBUTE-DEFINITION-STRING-REF",id);x.writeEndElement();x.writeEndElement();};value("GESIER-CODE",q.value(1).toString());value("GESIER-DESCRIPTION",q.value(3).toString());value("GESIER-TYPE",q.value(6).toString());value("GESIER-STATUS",q.value(7).toString());value("GESIER-PT",q.value(8).toString());QSqlQuery aq(db);aq.prepare("SELECT ATTRIBUTE_ID,VALUE FROM REQIF_ATTRIBUTE WHERE REQ_ID=? AND ATTRIBUTE_ID NOT LIKE 'GESIER-%'");aq.addBindValue(localId);if(aq.exec())while(aq.next())value(aq.value(0).toString(),aq.value(1).toString());x.writeEndElement();x.writeStartElement("TYPE");x.writeTextElement("SPEC-OBJECT-TYPE-REF","GESIER-REQUIREMENT-TYPE");x.writeEndElement();x.writeEndElement();}x.writeEndElement();
  x.writeStartElement("SPEC-RELATIONS");QSqlQuery r(db);r.exec("SELECT L.ID,L.SOURCE_REQ_ID,L.TARGET_REQ_ID FROM REQUIREMENT_RELATION L");while(r.next()){if(!ids.contains(r.value(1).toInt())||!ids.contains(r.value(2).toInt()))continue;x.writeStartElement("SPEC-RELATION");x.writeAttribute("IDENTIFIER",QString("GESIER-REL-%1").arg(r.value(0).toInt()));x.writeStartElement("SOURCE");x.writeTextElement("SPEC-OBJECT-REF",ids[r.value(1).toInt()]);x.writeEndElement();x.writeStartElement("TARGET");x.writeTextElement("SPEC-OBJECT-REF",ids[r.value(2).toInt()]);x.writeEndElement();x.writeStartElement("TYPE");x.writeTextElement("SPEC-RELATION-TYPE-REF","GESIER-RELATION-TYPE");x.writeEndElement();x.writeEndElement();}x.writeEndElement();
  x.writeStartElement("SPECIFICATIONS");x.writeStartElement("SPECIFICATION");x.writeAttribute("IDENTIFIER","GESIER-SPECIFICATION");x.writeAttribute("LONG-NAME","GeSiER requirements");x.writeStartElement("CHILDREN");std::function<void(int)> hierarchy=[&](int id){x.writeStartElement("SPEC-HIERARCHY");x.writeAttribute("IDENTIFIER",QString("GESIER-HIER-%1").arg(id));x.writeStartElement("OBJECT");x.writeTextElement("SPEC-OBJECT-REF",ids[id]);x.writeEndElement();QList<int> children;for(auto it=parents.cbegin();it!=parents.cend();++it)if(it.value()==id)children<<it.key();if(!children.isEmpty()){x.writeStartElement("CHILDREN");for(int child:children)hierarchy(child);x.writeEndElement();}x.writeEndElement();};for(auto it=parents.cbegin();it!=parents.cend();++it)if(it.value()<0||!ids.contains(it.value()))hierarchy(it.key());x.writeEndElement();x.writeStartElement("TYPE");x.writeTextElement("SPECIFICATION-TYPE-REF","GESIER-SPECIFICATION-TYPE");x.writeEndElement();x.writeEndElement();x.writeEndElement();
  x.writeEndElement();x.writeEndElement();x.writeEndElement();x.writeEndDocument();if(!file.commit()){if(error)*error=file.errorString();return false;}QSqlQuery audit(db);audit.prepare("INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,AFTER_JSON,COMMENT) VALUES('EXPORT','REQIF',json_object('file',?),?)");audit.addBindValue(QFileInfo(fileName).fileName());audit.addBindValue(QObject::tr("Export ReqIF"));if(!audit.exec()){if(error)*error=audit.lastError().text();return false;}return true;
}
