#include "applicabilityservice.h"

#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamWriter>
#include <private/qzipwriter_p.h>

namespace {
QString columnName(int column) {
  QString result;
  for (++column; column; column = (column - 1) / 26)
    result.prepend(QChar('A' + (column - 1) % 26));
  return result;
}
QByteArray sheetXml(const QList<QStringList> &rows) {
  QByteArray data;
  QXmlStreamWriter x(&data);
  x.writeStartDocument();
  x.writeStartElement("worksheet");
  x.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
  x.writeStartElement("sheetData");
  for (int r = 0; r < rows.size(); ++r) {
    x.writeStartElement("row"); x.writeAttribute("r", QString::number(r + 1));
    for (int c = 0; c < rows[r].size(); ++c) {
      x.writeStartElement("c"); x.writeAttribute("r", columnName(c) + QString::number(r + 1));
      x.writeAttribute("t", "inlineStr"); x.writeStartElement("is"); x.writeTextElement("t", rows[r][c]);
      x.writeEndElement(); x.writeEndElement();
    }
    x.writeEndElement();
  }
  x.writeEndElement(); x.writeEndElement(); x.writeEndDocument();
  return data;
}
bool writeWorkbook(const QString &path, const QList<QPair<QString,QList<QStringList>>> &sheets,
                   QString *error) {
  QZipWriter zip(path);
  if (zip.status() != QZipWriter::NoError) { if (error) *error = "Impossible de créer le fichier XLSX."; return false; }
  QByteArray types = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>";
  for (int i=0;i<sheets.size();++i) types += "<Override PartName=\"/xl/worksheets/sheet"+QByteArray::number(i+1)+".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
  types += "</Types>";
  zip.addFile("[Content_Types].xml", types);
  zip.addFile("_rels/.rels", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>");
  QByteArray workbook = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>";
  QByteArray rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
  for (int i=0;i<sheets.size();++i) {
    QString safe=sheets[i].first.left(31); safe.replace('&',"&amp;").replace('<',"&lt;").replace('>',"&gt;").replace('"',"&quot;");
    workbook += "<sheet name=\""+safe.toUtf8()+"\" sheetId=\""+QByteArray::number(i+1)+"\" r:id=\"rId"+QByteArray::number(i+1)+"\"/>";
    rels += "<Relationship Id=\"rId"+QByteArray::number(i+1)+"\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet"+QByteArray::number(i+1)+".xml\"/>";
    zip.addFile("xl/worksheets/sheet"+QByteArray::number(i+1)+".xml", sheetXml(sheets[i].second));
  }
  workbook += "</sheets></workbook>"; rels += "</Relationships>";
  zip.addFile("xl/workbook.xml", workbook); zip.addFile("xl/_rels/workbook.xml.rels", rels); zip.close();
  if (zip.status()!=QZipWriter::NoError || !QFileInfo(path).exists() || QFileInfo(path).size()==0) { if(error)*error="Échec de finalisation du XLSX."; return false; }
  return true;
}
}

ApplicabilityService::ApplicabilityService(QString name) : m_connectionName(std::move(name)) {}
void ApplicabilityService::setConnectionName(const QString &name) { m_connectionName=name; }

QList<ConfigurationRecord> ApplicabilityService::configurations(bool includeArchived, QString *error) const {
  QSqlQuery q(QSqlDatabase::database(m_connectionName));
  QString sql="SELECT C.ID,C.CODE,C.LABEL,C.DESCRIPTION,C.POSITION,C.ACTIVE,C.CREATED_AT,C.UPDATED_AT,(SELECT COUNT(DISTINCT A.REQ_ID) FROM REQUIREMENT_APPLICABILITY A WHERE A.CONFIG_ID=C.ID) FROM CONFIGURATION C";
  if(!includeArchived) sql+=" WHERE C.ACTIVE=1";
  sql+=" ORDER BY C.POSITION,C.CODE";
  QList<ConfigurationRecord> out;
  if(!q.exec(sql)){if(error)*error=q.lastError().text();return out;}
  while(q.next()){ConfigurationRecord c;c.id=q.value(0).toInt();c.code=q.value(1).toString();c.label=q.value(2).toString();c.description=q.value(3).toString();c.position=q.value(4).toInt();c.active=q.value(5).toBool();c.createdAt=q.value(6).toString();c.updatedAt=q.value(7).toString();c.useCount=q.value(8).toInt();out<<c;} return out;
}
ConfigurationRecord ApplicabilityService::configuration(int id) const { for(const auto&c:configurations(true))if(c.id==id)return c;return {}; }

RequirementResult ApplicabilityService::saveConfiguration(const ConfigurationRecord &c) {
  if(c.code.trimmed().isEmpty()||c.label.trimmed().isEmpty())return RequirementResult::failure("Le code et le libellé sont obligatoires.");
  QSqlDatabase db=QSqlDatabase::database(m_connectionName); QSqlQuery q(db);
  if(c.id<0){q.prepare("INSERT INTO CONFIGURATION(CODE,LABEL,DESCRIPTION,POSITION,ACTIVE,CREATED_AT,UPDATED_AT) VALUES(?,?,?,?,?,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP)");}
  else q.prepare("UPDATE CONFIGURATION SET CODE=?,LABEL=?,DESCRIPTION=?,POSITION=?,ACTIVE=?,UPDATED_AT=CURRENT_TIMESTAMP WHERE ID=?");
  q.addBindValue(c.code.trimmed().toUpper());q.addBindValue(c.label.trimmed());q.addBindValue(c.description);q.addBindValue(c.position);q.addBindValue(c.active?1:0);if(c.id>=0)q.addBindValue(c.id);
  if(!q.exec())return RequirementResult::failure(q.lastError().text());
  return RequirementResult::successResult("Configuration enregistrée.",c.id<0?q.lastInsertId().toInt():c.id,c.code);
}
RequirementResult ApplicabilityService::setConfigurationActive(int id,bool active){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare("UPDATE CONFIGURATION SET ACTIVE=?,UPDATED_AT=CURRENT_TIMESTAMP WHERE ID=?");q.addBindValue(active);q.addBindValue(id);if(!q.exec())return RequirementResult::failure(q.lastError().text());return RequirementResult::successResult(active?"Configuration restaurée.":"Configuration archivée.",id);}

QList<ApplicabilityRow> ApplicabilityService::matrix(const ApplicabilityFilter &f,QString *error) const {
  QSqlDatabase db=QSqlDatabase::database(m_connectionName); QStringList where{"1=1"}; QList<QVariant> args;
  if(f.documentId>=0){where<<"EXISTS(SELECT 1 FROM DOCUMENT_NODE DN WHERE DN.REQ_ID=R.ID AND DN.DOC_ID=?)";args<<f.documentId;}
  if(f.statusId>=0){where<<"R.STATUS=?";args<<f.statusId;} if(f.typeId>=0){where<<"R.TYPE=?";args<<f.typeId;}
  if(!f.includeObsoleteRequirements)where<<"NOT EXISTS(SELECT 1 FROM REQ_STATUS OS WHERE OS.ID=R.STATUS AND (UPPER(OS.STATUS)='OBSOLETE' OR UPPER(OS.SHORTCUT)='O'))";
  if(f.ptRootId>=0){where<<"EXISTS(WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL SELECT P.ID FROM PT P JOIN D ON P.PARENT=D.ID) SELECT 1 FROM REQUIREMENT_PT RP WHERE RP.REQ_ID=R.ID AND RP.PT_ID IN(SELECT ID FROM D))";args<<f.ptRootId;}
  QSqlQuery q(db);q.prepare("SELECT R.ID,R.CODE,R.TITLE,COALESCE(S.STATUS,''),COALESCE(T.TYPE,''),COALESCE((SELECT GROUP_CONCAT(P.NAME,', ') FROM REQUIREMENT_PT RP JOIN PT P ON P.ID=RP.PT_ID WHERE RP.REQ_ID=R.ID),''),COALESCE((SELECT GROUP_CONCAT(D.REFERENCE||' — '||D.TITLE,', ') FROM DOCUMENT_NODE N JOIN DOCUMENT D ON D.ID=N.DOC_ID WHERE N.REQ_ID=R.ID),'') FROM REQUIREMENT R LEFT JOIN REQ_STATUS S ON S.ID=R.STATUS LEFT JOIN REQ_TYPE T ON T.ID=R.TYPE WHERE "+where.join(" AND ")+" ORDER BY R.CODE");for(const auto&a:args)q.addBindValue(a);
  QList<ApplicabilityRow> rows;if(!q.exec()){if(error)*error=q.lastError().text();return rows;} const auto configs=configurations(f.includeArchivedConfigurations);
  struct EffectiveValue { bool applicable=false; int ptId=-1; int depth=1000000; QString comment; };
  QHash<int,int> ancestorDepth;
  if(f.ptRootId>=0){QSqlQuery ancestors(db);ancestors.prepare("WITH RECURSIVE A(ID,PARENT,DEPTH) AS(SELECT ID,PARENT,0 FROM PT WHERE ID=? UNION ALL SELECT P.ID,P.PARENT,A.DEPTH+1 FROM PT P JOIN A ON A.PARENT=P.ID) SELECT ID,DEPTH FROM A");ancestors.addBindValue(f.ptRootId);if(ancestors.exec())while(ancestors.next())ancestorDepth.insert(ancestors.value(0).toInt(),ancestors.value(1).toInt());}
  QHash<QString,EffectiveValue> effective;
  QSqlQuery values(db);
  if(values.exec("SELECT REQ_ID,CONFIG_ID,PT_ID,APPLICABLE,COALESCE(COMMENT,'') FROM REQUIREMENT_APPLICABILITY"))while(values.next()){
    const int ptId=values.value(2).isNull()?-1:values.value(2).toInt();
    int depth=1000000;
    if(ptId<0) depth=999999;
    else if(f.ptRootId<0 || !ancestorDepth.contains(ptId)) continue;
    else depth=ancestorDepth.value(ptId);
    const QString key=QString::number(values.value(0).toInt())+'|'+QString::number(values.value(1).toInt());
    if(!effective.contains(key)||depth<effective[key].depth)effective.insert(key,{values.value(3).toBool(),ptId,depth,values.value(4).toString()});
  }
  while(q.next()){ApplicabilityRow r;r.requirementId=q.value(0).toInt();r.code=q.value(1).toString();r.title=q.value(2).toString();r.status=q.value(3).toString();r.type=q.value(4).toString();r.productTrees=q.value(5).toString();r.document=q.value(6).toString();
    for(const auto&c:configs){ApplicabilityCell cell;cell.requirementId=r.requirementId;cell.configurationId=c.id;const QString key=QString::number(r.requirementId)+'|'+QString::number(c.id);if(effective.contains(key)){const auto value=effective.value(key);cell.applicable=value.applicable;cell.comment=value.comment;cell.sourcePtId=value.ptId;cell.explicitValue=true;}r.cells<<cell;}
    rows<<r;}return rows;
}
RequirementResult ApplicabilityService::setApplicability(int req,int config,int pt,bool value,const QString&comment){QSqlQuery q(QSqlDatabase::database(m_connectionName));if(pt<0)q.prepare("INSERT INTO REQUIREMENT_APPLICABILITY(REQ_ID,CONFIG_ID,PT_ID,APPLICABLE,COMMENT) VALUES(?,?,NULL,?,?) ON CONFLICT(REQ_ID,CONFIG_ID) WHERE PT_ID IS NULL DO UPDATE SET APPLICABLE=excluded.APPLICABLE,COMMENT=excluded.COMMENT");else q.prepare("INSERT INTO REQUIREMENT_APPLICABILITY(REQ_ID,CONFIG_ID,PT_ID,APPLICABLE,COMMENT) VALUES(?,?,?,?,?) ON CONFLICT(REQ_ID,CONFIG_ID,PT_ID) DO UPDATE SET APPLICABLE=excluded.APPLICABLE,COMMENT=excluded.COMMENT");q.addBindValue(req);q.addBindValue(config);if(pt>=0)q.addBindValue(pt);q.addBindValue(value);q.addBindValue(comment);if(!q.exec())return RequirementResult::failure(q.lastError().text());return RequirementResult::successResult("Applicabilité enregistrée.",req);}
RequirementResult ApplicabilityService::clearOverride(int req,int config,int pt){QSqlQuery q(QSqlDatabase::database(m_connectionName));if(pt<0)q.prepare("DELETE FROM REQUIREMENT_APPLICABILITY WHERE REQ_ID=? AND CONFIG_ID=? AND PT_ID IS NULL");else q.prepare("DELETE FROM REQUIREMENT_APPLICABILITY WHERE REQ_ID=? AND CONFIG_ID=? AND PT_ID=?");q.addBindValue(req);q.addBindValue(config);if(pt>=0)q.addBindValue(pt);if(!q.exec())return RequirementResult::failure(q.lastError().text());return RequirementResult::successResult("Valeur retirée.",req);}
double ApplicabilityService::rate(const QList<ApplicabilityRow>&rows)const{int yes=0,total=0;for(const auto&r:rows)for(const auto&c:r.cells){++total;if(c.applicable)++yes;}return total?100.0*yes/total:0.;}

RequirementResult ApplicabilityService::exportXlsx(const QString&path,const ApplicabilityFilter&filter)const{QString error;const auto configs=configurations(filter.includeArchivedConfigurations,&error);const auto rows=matrix(filter,&error);if(!error.isEmpty())return RequirementResult::failure(error);QList<QStringList> summary{{"Indicateur","Valeur"},{"Exigences",QString::number(rows.size())},{"Configurations",QString::number(configs.size())},{"Taux d'applicabilité",QString::number(rate(rows),'f',2)+" %"}};QList<QStringList> details;QStringList head{"Code","Titre","Statut","Type","Product Trees","Documents"};for(const auto&c:configs)head<<c.code+(c.active?QString():" [archivée]");details<<head;QList<QStringList> anomalies{{"Code","Anomalie"}};for(const auto&r:rows){QStringList line{r.code,r.title,r.status,r.type,r.productTrees,r.document};int yes=0;for(const auto&c:r.cells){line<<(c.applicable?"Oui":"Non");if(c.applicable)++yes;}details<<line;if(configs.isEmpty())anomalies<<QStringList{r.code,"Aucune configuration définie"};else if(!yes)anomalies<<QStringList{r.code,"Non applicable à toutes les configurations"};}if(anomalies.size()==1)anomalies<<QStringList{"—","Aucune anomalie"};if(!writeWorkbook(path,{{"Synthèse",summary},{"Détails",details},{"Anomalies",anomalies}},&error))return RequirementResult::failure(error);return RequirementResult::successResult("Matrice XLSX exportée et vérifiée.",-1,path);}
