#include "documentservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <functional>

namespace {
QVariant parentValue(int id) { return id < 0 ? QVariant() : QVariant(id); }
RequirementResult rollback(QSqlDatabase &db, const QString &error) {
  db.rollback(); return RequirementResult::failure(error);
}
bool logChange(QSqlDatabase db, int doc, int node, const QString &comment,
               QString *error) {
  QSqlQuery q(db);
  q.prepare("INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON,COMMENT) VALUES('COMPOSITION','DOCUMENT',?,json_object('node_id',?),?)");
  q.addBindValue(doc); q.addBindValue(node); q.addBindValue(comment);
  if (q.exec()) return true;
  *error = q.lastError().text(); return false;
}
bool checkParent(QSqlDatabase db, int doc, int parent, QString *error) {
  if (parent < 0) return true;
  QSqlQuery q(db);
  q.prepare("SELECT 1 FROM DOCUMENT_NODE WHERE ID=? AND DOC_ID=? AND NODE_TYPE='CHAPTER'");
  q.addBindValue(parent); q.addBindValue(doc);
  if (q.exec() && q.next()) return true;
  *error = q.lastError().isValid() ? q.lastError().text()
                                 : "Le parent doit être un chapitre du même document.";
  return false;
}
bool normalize(QSqlDatabase db, int doc, int parent, QString *error) {
  QSqlQuery q(db);
  q.prepare("SELECT ID FROM DOCUMENT_NODE WHERE DOC_ID=? AND PARENT_ID IS ? ORDER BY POSITION,ID");
  q.addBindValue(doc); q.addBindValue(parentValue(parent));
  if (!q.exec()) { *error=q.lastError().text(); return false; }
  QList<int> ids; while(q.next()) ids << q.value(0).toInt();
  QSqlQuery park(db);
  park.prepare("UPDATE DOCUMENT_NODE SET POSITION=-ID WHERE DOC_ID=? AND PARENT_ID IS ?");
  park.addBindValue(doc); park.addBindValue(parentValue(parent));
  if (!park.exec()) { *error=park.lastError().text(); return false; }
  for (int i=0;i<ids.size();++i) {
    QSqlQuery u(db); u.prepare("UPDATE DOCUMENT_NODE SET POSITION=? WHERE ID=?");
    u.addBindValue(i); u.addBindValue(ids[i]);
    if (!u.exec()) { *error=u.lastError().text(); return false; }
  }
  return true;
}
RequirementResult insertNode(const QString &connection, int doc, int parent,
                             const QString &type, const QString &title,
                             int requirement, const QString &text,
                             const QByteArray &image, const QString &legend) {
  QSqlDatabase db=QSqlDatabase::database(connection);
  if (!db.transaction()) return RequirementResult::failure(db.lastError().text());
  QString error;
  if (!checkParent(db,doc,parent,&error)) return rollback(db,error);
  QSqlQuery q(db);
  q.prepare("INSERT INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,NODE_TYPE,POSITION,TITLE,REQ_ID,TEXT_CONTENT,IMAGE_DATA,IMAGE_LEGEND) VALUES(?,?,?,(SELECT COUNT(*) FROM DOCUMENT_NODE WHERE DOC_ID=? AND PARENT_ID IS ?),?,?,?,?,?)");
  q.addBindValue(doc); q.addBindValue(parentValue(parent)); q.addBindValue(type);
  q.addBindValue(doc); q.addBindValue(parentValue(parent)); q.addBindValue(title);
  q.addBindValue(requirement<0?QVariant():QVariant(requirement));
  q.addBindValue(text); q.addBindValue(image); q.addBindValue(legend);
  if (!q.exec()) return rollback(db,q.lastError().text());
  const int id=q.lastInsertId().toInt();
  if (!logChange(db,doc,id,"Ajout "+type,&error)) return rollback(db,error);
  if (!db.commit()) return RequirementResult::failure(db.lastError().text());
  return RequirementResult::successResult("Élément ajouté.",id);
}
RequirementResult editNode(const QString &connection, int node,
                           const QString &sql, const QVariantList &values,
                           const QString &message) {
  QSqlDatabase db=QSqlDatabase::database(connection);
  if (!db.transaction()) return RequirementResult::failure(db.lastError().text());
  QSqlQuery find(db); find.prepare("SELECT DOC_ID FROM DOCUMENT_NODE WHERE ID=?");
  find.addBindValue(node);
  if (!find.exec() || !find.next()) return rollback(db,"Élément introuvable.");
  const int doc=find.value(0).toInt();
  QSqlQuery q(db); q.prepare(sql); for(const QVariant &v:values) q.addBindValue(v); q.addBindValue(node);
  if (!q.exec() || q.numRowsAffected()!=1)
    return rollback(db,q.lastError().isValid()?q.lastError().text():"Type d'élément incorrect.");
  QString error; if(!logChange(db,doc,node,message,&error)) return rollback(db,error);
  if(!db.commit()) return RequirementResult::failure(db.lastError().text());
  return RequirementResult::successResult(message,node);
}
}

DocumentService::DocumentService(QString connectionName):m_connectionName(std::move(connectionName)){}

QList<DocumentRecord> DocumentService::documents() const {
  QList<DocumentRecord> out;
  QSqlQuery q("SELECT ID,PT_ID,TYPE,COALESCE(REFERENCE,''),TITLE,COALESCE(DESCRIPTION,'') FROM DOCUMENT ORDER BY TITLE",QSqlDatabase::database(m_connectionName));
  while(q.next()){DocumentRecord r;r.id=q.value(0).toInt();r.ptId=q.value(1).toInt();r.typeId=q.value(2).toInt();r.reference=q.value(3).toString();r.title=q.value(4).toString();r.description=q.value(5).toString();out<<r;} return out;
}
DocumentRecord DocumentService::document(int id) const {
  DocumentRecord r; QSqlQuery q(QSqlDatabase::database(m_connectionName));
  q.prepare("SELECT ID,PT_ID,TYPE,COALESCE(REFERENCE,''),TITLE,COALESCE(DESCRIPTION,''),COALESCE(METADATA_JSON,'{}') FROM DOCUMENT WHERE ID=?");q.addBindValue(id);
  if(!q.exec()||!q.next())return r;
  r.id=q.value(0).toInt();r.ptId=q.value(1).toInt();r.typeId=q.value(2).toInt();r.reference=q.value(3).toString();r.title=q.value(4).toString();r.description=q.value(5).toString();
  const auto object=QJsonDocument::fromJson(q.value(6).toByteArray()).object();for(auto it=object.begin();it!=object.end();++it)r.metadata[it.key()]=it.value().toVariant().toString();
  QSqlQuery refs(QSqlDatabase::database(m_connectionName));refs.prepare("SELECT REFERENCE FROM DOCUMENT_REFERENCE WHERE DOC_ID=? AND IS_PRIMARY=0 ORDER BY ID");refs.addBindValue(id);if(refs.exec())while(refs.next())r.secondaryReferences<<refs.value(0).toString();return r;
}
QList<DocumentNodeRecord> DocumentService::nodes(int doc) const {
  QList<DocumentNodeRecord> out;QSqlQuery q(QSqlDatabase::database(m_connectionName));
  q.prepare("SELECT N.ID,N.DOC_ID,N.PARENT_ID,N.POSITION,N.NODE_TYPE,COALESCE(N.TITLE,''),N.REQ_ID,COALESCE(R.CODE,''),COALESCE(R.TITLE,''),COALESCE(N.TEXT_CONTENT,''),N.IMAGE_DATA,COALESCE(N.IMAGE_LEGEND,'') FROM DOCUMENT_NODE N LEFT JOIN REQUIREMENT R ON R.ID=N.REQ_ID WHERE N.DOC_ID=? ORDER BY N.PARENT_ID,N.POSITION,N.ID");q.addBindValue(doc);if(!q.exec())return out;
  while(q.next()){DocumentNodeRecord n;n.id=q.value(0).toInt();n.documentId=q.value(1).toInt();n.parentId=q.value(2).isNull()?-1:q.value(2).toInt();n.position=q.value(3).toInt();n.type=q.value(4).toString();n.title=q.value(5).toString();n.requirementId=q.value(6).isNull()?-1:q.value(6).toInt();n.requirementCode=q.value(7).toString();n.requirementTitle=q.value(8).toString();n.textContent=q.value(9).toString();n.imageData=q.value(10).toByteArray();n.imageLegend=q.value(11).toString();out<<n;}return out;
}
RequirementResult DocumentService::saveDocument(const DocumentRecord&r){
  if(r.title.trimmed().isEmpty()||r.reference.trimmed().isEmpty())
    return RequirementResult::failure("La référence et le titre sont obligatoires.");
  QSqlDatabase db=QSqlDatabase::database(m_connectionName);
  if(!db.transaction())return RequirementResult::failure(db.lastError().text());
  QJsonObject object;for(auto it=r.metadata.cbegin();it!=r.metadata.cend();++it)object[it.key()]=it.value();const QString json=QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));int id=r.id;QSqlQuery q(db);
  if(id<0){q.prepare("INSERT INTO DOCUMENT(PT_ID,TYPE,REFERENCE,TITLE,DESCRIPTION,METADATA_JSON) VALUES(?,?,?,?,?,?)");q.addBindValue(r.ptId);q.addBindValue(r.typeId);q.addBindValue(r.reference.trimmed());q.addBindValue(r.title.trimmed());q.addBindValue(r.description);q.addBindValue(json);if(q.exec())id=q.lastInsertId().toInt();}
  else{q.prepare("UPDATE DOCUMENT SET PT_ID=?,TYPE=?,REFERENCE=?,TITLE=?,DESCRIPTION=?,METADATA_JSON=? WHERE ID=?");q.addBindValue(r.ptId);q.addBindValue(r.typeId);q.addBindValue(r.reference.trimmed());q.addBindValue(r.title.trimmed());q.addBindValue(r.description);q.addBindValue(json);q.addBindValue(id);q.exec();}
  if(q.lastError().isValid())return rollback(db,q.lastError().text());
  QSqlQuery clear(db);clear.prepare("DELETE FROM DOCUMENT_REFERENCE WHERE DOC_ID=?");clear.addBindValue(id);if(!clear.exec())return rollback(db,clear.lastError().text());QStringList refs=r.secondaryReferences;refs.prepend(r.reference.trimmed());
  for(int i=0;i<refs.size();++i){const QString ref=refs[i].trimmed();if(ref.isEmpty())continue;QSqlQuery add(db);add.prepare("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(?,?,?)");add.addBindValue(id);add.addBindValue(ref);add.addBindValue(i==0);if(!add.exec())return rollback(db,add.lastError().text());}
  if(!db.commit())return RequirementResult::failure(db.lastError().text());
  return RequirementResult::successResult("Document enregistré.",id);
}
RequirementResult DocumentService::addChapter(int d,int p,const QString&t){if(t.trimmed().isEmpty())return RequirementResult::failure("Le titre du chapitre est obligatoire.");return insertNode(m_connectionName,d,p,"CHAPTER",t.trimmed(),-1,{},{},{});}
RequirementResult DocumentService::placeRequirement(int d,int p,int r){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare("SELECT ID FROM DOCUMENT_NODE WHERE DOC_ID=? AND REQ_ID=? AND NODE_TYPE='REQUIREMENT'");q.addBindValue(d);q.addBindValue(r);if(q.exec()&&q.next())return moveNode(q.value(0).toInt(),p,-1);return insertNode(m_connectionName,d,p,"REQUIREMENT",{},r,{},{},{});}
RequirementResult DocumentService::addText(int d,int p,const QString&h){if(h.trimmed().isEmpty())return RequirementResult::failure("Le texte est vide.");return insertNode(m_connectionName,d,p,"TEXT",{},-1,h,{},{});}
RequirementResult DocumentService::addImage(int d,int p,const QByteArray&b,const QString&l){if(b.isEmpty())return RequirementResult::failure("L'image est vide.");return insertNode(m_connectionName,d,p,"IMAGE",{},-1,{},b,l.trimmed());}
RequirementResult DocumentService::moveNode(int node,int parent,int position){
  QSqlDatabase db=QSqlDatabase::database(m_connectionName);if(!db.transaction())return RequirementResult::failure(db.lastError().text());QSqlQuery find(db);find.prepare("SELECT DOC_ID,PARENT_ID FROM DOCUMENT_NODE WHERE ID=?");find.addBindValue(node);if(!find.exec()||!find.next())return rollback(db,"Élément introuvable.");const int doc=find.value(0).toInt(),old=find.value(1).isNull()?-1:find.value(1).toInt();QString error;if(!checkParent(db,doc,parent,&error))return rollback(db,error);if(parent==node)return rollback(db,"Un élément ne peut pas être son propre parent.");
  QSqlQuery cycle(db);cycle.prepare("WITH RECURSIVE D(ID) AS (SELECT ID FROM DOCUMENT_NODE WHERE PARENT_ID=? UNION ALL SELECT N.ID FROM DOCUMENT_NODE N JOIN D ON N.PARENT_ID=D.ID) SELECT 1 FROM D WHERE ID=?");cycle.addBindValue(node);cycle.addBindValue(parent);if(!cycle.exec())return rollback(db,cycle.lastError().text());if(cycle.next())return rollback(db,"Cycle de document interdit.");
  QSqlQuery list(db);list.prepare("SELECT ID FROM DOCUMENT_NODE WHERE DOC_ID=? AND PARENT_ID IS ? AND ID<>? ORDER BY POSITION,ID");list.addBindValue(doc);list.addBindValue(parentValue(parent));list.addBindValue(node);if(!list.exec())return rollback(db,list.lastError().text());QList<int> ids;while(list.next())ids<<list.value(0).toInt();position=qBound(0,position<0?ids.size():position,ids.size());ids.insert(position,node);
  QSqlQuery park(db);park.prepare("UPDATE DOCUMENT_NODE SET POSITION=-ID WHERE DOC_ID=? AND (PARENT_ID IS ? OR PARENT_ID IS ?)");park.addBindValue(doc);park.addBindValue(parentValue(old));park.addBindValue(parentValue(parent));if(!park.exec())return rollback(db,park.lastError().text());QSqlQuery move(db);move.prepare("UPDATE DOCUMENT_NODE SET PARENT_ID=? WHERE ID=?");move.addBindValue(parentValue(parent));move.addBindValue(node);if(!move.exec())return rollback(db,move.lastError().text());for(int i=0;i<ids.size();++i){QSqlQuery u(db);u.prepare("UPDATE DOCUMENT_NODE SET POSITION=? WHERE ID=?");u.addBindValue(i);u.addBindValue(ids[i]);if(!u.exec())return rollback(db,u.lastError().text());}if(old!=parent&&!normalize(db,doc,old,&error))return rollback(db,error);if(!logChange(db,doc,node,"Déplacement",&error))return rollback(db,error);if(!db.commit())return RequirementResult::failure(db.lastError().text());return RequirementResult::successResult("Élément déplacé.",node);
}
RequirementResult DocumentService::renameChapter(int n,const QString&t){if(t.trimmed().isEmpty())return RequirementResult::failure("Le titre du chapitre est obligatoire.");return editNode(m_connectionName,n,"UPDATE DOCUMENT_NODE SET TITLE=? WHERE ID=? AND NODE_TYPE='CHAPTER'",{t.trimmed()},"Chapitre renommé.");}
RequirementResult DocumentService::updateText(int n,const QString&h){if(h.trimmed().isEmpty())return RequirementResult::failure("Le texte est vide.");return editNode(m_connectionName,n,"UPDATE DOCUMENT_NODE SET TEXT_CONTENT=? WHERE ID=? AND NODE_TYPE='TEXT'",{h},"Texte modifié.");}
RequirementResult DocumentService::updateImage(int n,const QByteArray&b,const QString&l){if(b.isEmpty())return RequirementResult::failure("L'image est vide.");return editNode(m_connectionName,n,"UPDATE DOCUMENT_NODE SET IMAGE_DATA=?,IMAGE_LEGEND=? WHERE ID=? AND NODE_TYPE='IMAGE'",{b,l.trimmed()},"Image modifiée.");}
RequirementResult DocumentService::removeNode(int node){QSqlDatabase db=QSqlDatabase::database(m_connectionName);if(!db.transaction())return RequirementResult::failure(db.lastError().text());QSqlQuery find(db);find.prepare("SELECT DOC_ID,PARENT_ID FROM DOCUMENT_NODE WHERE ID=?");find.addBindValue(node);if(!find.exec()||!find.next())return rollback(db,"Élément introuvable.");const int doc=find.value(0).toInt(),parent=find.value(1).isNull()?-1:find.value(1).toInt();QSqlQuery del(db);del.prepare("DELETE FROM DOCUMENT_NODE WHERE ID=?");del.addBindValue(node);if(!del.exec())return rollback(db,del.lastError().text());QString error;if(!normalize(db,doc,parent,&error)||!logChange(db,doc,node,"Suppression",&error))return rollback(db,error);if(!db.commit())return RequirementResult::failure(db.lastError().text());return RequirementResult::successResult("Élément retiré.",node);}
QString DocumentService::previewHtml(int docId)const{const DocumentRecord doc=document(docId);const QList<DocumentNodeRecord> all=nodes(docId);QMap<int,QList<DocumentNodeRecord>> children;for(const auto&n:all)children[n.parentId]<<n;std::function<QString(int,int)> render=[&](int p,int level){QString h;for(const auto&n:children.value(p)){if(n.type=="CHAPTER")h+=QString("<h%1>%2</h%1>").arg(qMin(level+2,6)).arg(n.title.toHtmlEscaped());else if(n.type=="REQUIREMENT")h+="<p><b>"+(n.requirementCode+" — "+n.requirementTitle).toHtmlEscaped()+"</b></p>";else if(n.type=="TEXT")h+=n.textContent;else if(n.type=="IMAGE")h+="<figure><img style=\"max-width:100%\" src=\"data:image/png;base64,"+QString::fromLatin1(n.imageData.toBase64())+"\"><figcaption>"+n.imageLegend.toHtmlEscaped()+"</figcaption></figure>";h+=render(n.id,level+1);}return h;};return "<html><body><h1>"+doc.title.toHtmlEscaped()+"</h1><p><b>"+doc.reference.toHtmlEscaped()+"</b></p>"+render(-1,0)+"</body></html>";}
