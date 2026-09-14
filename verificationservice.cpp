#include "verificationservice.h"

#include <QFileInfo>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamWriter>
#include <private/qzipwriter_p.h>

namespace {
QByteArray sheetXml(const QList<QStringList> &rows) {
  QByteArray data; QXmlStreamWriter x(&data); x.writeStartDocument();
  x.writeStartElement("worksheet");
  x.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
  x.writeStartElement("sheetData");
  for (int r=0;r<rows.size();++r) { x.writeStartElement("row"); x.writeAttribute("r",QString::number(r+1));
    for(int c=0;c<rows[r].size();++c){x.writeStartElement("c");x.writeAttribute("r",QString(QChar('A'+c))+QString::number(r+1));x.writeAttribute("t","inlineStr");x.writeStartElement("is");x.writeTextElement("t",rows[r][c]);x.writeEndElement();x.writeEndElement();}
    x.writeEndElement(); }
  x.writeEndElement(); x.writeEndElement(); x.writeEndDocument(); return data;
}
bool writeWorkbook(const QString &path,const QList<QPair<QString,QList<QStringList>>> &sheets,QString *error){
  QZipWriter zip(path); if(zip.status()!=QZipWriter::NoError){if(error)*error="Impossible de créer le fichier XLSX.";return false;}
  QByteArray types="<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>";
  for(int i=0;i<sheets.size();++i)
    types+="<Override PartName=\"/xl/worksheets/sheet"+QByteArray::number(i+1)+".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
  types+="</Types>";zip.addFile("[Content_Types].xml",types);
  zip.addFile("_rels/.rels","<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>");
  QByteArray wb="<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>", rel="<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
  for(int i=0;i<sheets.size();++i){wb+="<sheet name=\""+sheets[i].first.toHtmlEscaped().toUtf8()+"\" sheetId=\""+QByteArray::number(i+1)+"\" r:id=\"rId"+QByteArray::number(i+1)+"\"/>";rel+="<Relationship Id=\"rId"+QByteArray::number(i+1)+"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet"+QByteArray::number(i+1)+".xml\"/>";zip.addFile("xl/worksheets/sheet"+QByteArray::number(i+1)+".xml",sheetXml(sheets[i].second));}
  wb+="</sheets></workbook>";rel+="</Relationships>";zip.addFile("xl/workbook.xml",wb);zip.addFile("xl/_rels/workbook.xml.rels",rel);zip.close();
  if(zip.status()!=QZipWriter::NoError||!QFileInfo(path).exists()||QFileInfo(path).size()==0){if(error)*error="Échec de finalisation du XLSX.";return false;}return true;
}
}

VerificationService::VerificationService(QString name):m_connectionName(std::move(name)){}
void VerificationService::setConnectionName(const QString&name){m_connectionName=name;}

QList<VerificationMatrixRow> VerificationService::matrix(const VerificationFilter &f,QString *error)const{
  QSqlDatabase db=QSqlDatabase::database(m_connectionName,false);QList<VerificationMatrixRow> out;
  if(!db.isValid()||!db.isOpen()){if(error)*error="La base de données n'est pas ouverte.";return out;}
  QString sql="SELECT R.ID,R.CODE,R.TITLE,S.STATUS,T.TYPE,COALESCE((SELECT GROUP_CONCAT(P.NAME, ', ') FROM REQUIREMENT_PT RP JOIN PT P ON P.ID=RP.PT_ID WHERE RP.REQ_ID=R.ID),''),COALESCE((SELECT GROUP_CONCAT(COALESCE(NULLIF(D.REFERENCE,''),D.TITLE), ', ') FROM DOCUMENT_NODE N JOIN DOCUMENT D ON D.ID=N.DOC_ID WHERE N.REQ_ID=R.ID AND N.NODE_TYPE='REQUIREMENT'),'' ) FROM REQUIREMENT R JOIN REQ_STATUS S ON S.ID=R.STATUS JOIN REQ_TYPE T ON T.ID=R.TYPE WHERE 1=1";
  QVariantList binds;if(!f.includeObsolete)sql+=" AND UPPER(COALESCE(S.STATUS,''))<>'OBSOLETE' AND UPPER(COALESCE(S.SHORTCUT,''))<>'O'";
  if(f.ptRootId>=0){sql+=" AND EXISTS(WITH RECURSIVE D(ID) AS (SELECT ? UNION ALL SELECT P.ID FROM PT P JOIN D ON P.PARENT=D.ID) SELECT 1 FROM REQUIREMENT_PT RP JOIN D ON D.ID=RP.PT_ID WHERE RP.REQ_ID=R.ID)";binds<<f.ptRootId;}
  if(f.documentId>=0){sql+=" AND EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE N.REQ_ID=R.ID AND N.DOC_ID=? AND N.NODE_TYPE='REQUIREMENT')";binds<<f.documentId;}
  if(f.statusId>=0){sql+=" AND R.STATUS=?";binds<<f.statusId;}if(f.typeId>=0){sql+=" AND R.TYPE=?";binds<<f.typeId;}
  if(f.methodId>=0){sql+=" AND EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V WHERE V.REQ_ID=R.ID AND V.METHOD_ID=?)";binds<<f.methodId;}sql+=" ORDER BY R.CODE";
  QSqlQuery q(db);q.prepare(sql);for(const auto&v:binds)q.addBindValue(v);if(!q.exec()){if(error)*error=q.lastError().text();return out;}
  while(q.next()){VerificationMatrixRow r;r.requirementId=q.value(0).toInt();r.code=q.value(1).toString();r.title=q.value(2).toString();r.status=q.value(3).toString();r.type=q.value(4).toString();r.productTrees=q.value(5).toString();r.documents=q.value(6).toString();
    QSqlQuery v(db);v.prepare("SELECT V.ID,V.METHOD_ID,V.VERIFICATION_LEVEL_PT_ID,COALESCE(P.NAME,V.VERIF_LEVEL,''),V.PROCEDURE_REF,V.REDMINE_REF,V.MEANS,V.VERDICT,V.COMMENT,V.POSITION,M.METHOD,COALESCE(P.ARCHIVED,0) FROM REQUIREMENT_VERIFICATION V JOIN REQ_METHOD M ON M.ID=V.METHOD_ID LEFT JOIN PT P ON P.ID=V.VERIFICATION_LEVEL_PT_ID WHERE V.REQ_ID=? ORDER BY V.POSITION,V.ID");v.addBindValue(r.requirementId);v.exec();QStringList methods,levels;
    while(v.next()){RequirementVerification x;x.id=v.value(0).toInt();x.methodId=v.value(1).toInt();x.levelPtId=v.value(2).isNull()?-1:v.value(2).toInt();x.level=v.value(3).toString();x.procedure=v.value(4).toString();x.redmine=v.value(5).toString();x.means=v.value(6).toString();x.verdict=v.value(7).toString();x.comment=v.value(8).toString();x.position=v.value(9).toInt();r.verifications<<x;methods<<v.value(10).toString();levels<<x.level+(v.value(11).toBool()?" [archivé]":"");if(x.methodId>=0&&x.levelPtId>=0&&!v.value(11).toBool())r.covered=true;}
    r.verificationCount=r.verifications.size();r.methods=methods.join(" ; ");r.levels=levels.join(" ; ");if(f.coverage<0||r.covered==(f.coverage==1))out<<r;}
  return out;
}

RequirementResult VerificationService::save(int req,const QList<RequirementVerification>&rows){
  QSet<int> methods;for(const auto&r:rows){if(r.methodId<0)return RequirementResult::failure("Chaque ligne doit avoir une méthode.");if(methods.contains(r.methodId))return RequirementResult::failure("Une méthode ne peut apparaître qu'une fois par exigence.");methods<<r.methodId;if(!r.verdict.isEmpty()&&!QStringList{"C","PC","NC"}.contains(r.verdict))return RequirementResult::failure("Verdict invalide.");if(r.levelPtId>=0&&r.id<0){QSqlQuery p(QSqlDatabase::database(m_connectionName));p.prepare("SELECT ARCHIVED FROM PT WHERE ID=?");p.addBindValue(r.levelPtId);if(!p.exec()||!p.next()||p.value(0).toBool())return RequirementResult::failure("Le niveau de vérification doit être un élément PT actif.");}}
  QSqlDatabase db=QSqlDatabase::database(m_connectionName);if(!db.transaction())return RequirementResult::failure(db.lastError().text());QSqlQuery del(db);del.prepare("DELETE FROM REQUIREMENT_VERIFICATION WHERE REQ_ID=?");del.addBindValue(req);if(!del.exec()){db.rollback();return RequirementResult::failure(del.lastError().text());}
  for(int i=0;i<rows.size();++i){const auto&r=rows[i];QSqlQuery ins(db);ins.prepare("INSERT INTO REQUIREMENT_VERIFICATION(REQ_ID,METHOD_ID,VERIFICATION_LEVEL_PT_ID,VERIF_LEVEL,PROCEDURE_REF,REDMINE_REF,MEANS,VERDICT,COMMENT,POSITION) VALUES(?,?,?,?,?,?,?,?,?,?)");ins.addBindValue(req);ins.addBindValue(r.methodId);ins.addBindValue(r.levelPtId<0?QVariant():QVariant(r.levelPtId));ins.addBindValue(r.level);ins.addBindValue(r.procedure);ins.addBindValue(r.redmine);ins.addBindValue(r.means);ins.addBindValue(r.verdict.isEmpty()?QVariant():QVariant(r.verdict));ins.addBindValue(r.comment);ins.addBindValue(i);if(!ins.exec()){db.rollback();return RequirementResult::failure(ins.lastError().text());}}
  if(!db.commit()){db.rollback();return RequirementResult::failure(db.lastError().text());}return RequirementResult::successResult("Plan de vérification enregistré.",req);
}
double VerificationService::coverageRate(const QList<VerificationMatrixRow>&rows)const{int n=0;for(const auto&r:rows)if(r.covered)++n;return rows.isEmpty()?0.:100.*n/rows.size();}
RequirementResult VerificationService::exportXlsx(const QString&path,const VerificationFilter&f)const{QString error;auto rows=matrix(f,&error);if(!error.isEmpty())return RequirementResult::failure(error);int covered=0;QList<QStringList> details{{"Exigence","Titre","Statut","Type","Méthode","Niveau PT","Procédure / cas de test","Référence Redmine","Moyen","Verdict","Commentaire"}},anomalies{{"Exigence","Anomalie"}};for(const auto&r:rows){if(r.covered)++covered;else anomalies<<QStringList{r.code,"Aucune ligne avec méthode et niveau PT actif"};if(r.verifications.isEmpty())details<<QStringList{r.code,r.title,r.status,r.type,"","","","","","",""};for(const auto&v:r.verifications){QSqlQuery m(QSqlDatabase::database(m_connectionName));m.prepare("SELECT METHOD FROM REQ_METHOD WHERE ID=?");m.addBindValue(v.methodId);m.exec();m.next();details<<QStringList{r.code,r.title,r.status,r.type,m.value(0).toString(),v.level,v.procedure,v.redmine,v.means,v.verdict,v.comment};}}if(anomalies.size()==1)anomalies<<QStringList{"—","Aucune anomalie"};QList<QStringList> summary{{"Indicateur","Valeur"},{"Exigences",QString::number(rows.size())},{"Couvertes",QString::number(covered)},{"Non couvertes",QString::number(rows.size()-covered)},{"Taux de couverture",QString::number(coverageRate(rows),'f',2)+" %"}};if(!writeWorkbook(path,{{"Synthèse",summary},{"Détails",details},{"Anomalies",anomalies}},&error))return RequirementResult::failure(error);return RequirementResult::successResult("Matrice de vérification exportée.",-1,path);}
