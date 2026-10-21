#include "auditservice.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
QJsonValue value(const QVariant &v) {
  return v.isNull() ? QJsonValue() : QJsonValue::fromVariant(v);
}

QJsonArray rows(QSqlDatabase db, const QString &sql, int id,
                const QStringList &keys) {
  QJsonArray result;
  QSqlQuery q(db);
  q.prepare(sql);
  q.addBindValue(id);
  if (!q.exec()) return result;
  while (q.next()) {
    QJsonObject row;
    for (int i = 0; i < keys.size(); ++i) row.insert(keys[i], value(q.value(i)));
    result.append(row);
  }
  return result;
}

void addSubject(QList<AuditSubject> &out, QSet<QString> &seen,
                const QString &type, int id, const QString &role = "AFFECTED") {
  if (id < 0) return;
  const QString key = type + ':' + QString::number(id);
  if (seen.contains(key)) return;
  seen.insert(key);
  out.append({type, id, role});
}
}

QJsonObject AuditService::requirementSnapshot(QSqlDatabase db, int id) {
  if (id < 0) return {};
  QSqlQuery q(db);
  q.prepare("SELECT R.CODE,R.TITLE,COALESCE(R.DESCRIPTION,''),"
            "R.TYPE,COALESCE(T.TYPE,''),R.STATUS,COALESCE(S.STATUS,''),"
            "COALESCE(R.SOURCE,''),COALESCE(R.IS_TRACE_ROOT,0),"
            "COALESCE(R.COMMENTS,''),COALESCE(R.NOT_APPLICABLE,0) FROM REQUIREMENT R LEFT JOIN REQ_TYPE T "
            "ON T.ID=R.TYPE LEFT JOIN REQ_STATUS S ON S.ID=R.STATUS WHERE R.ID=?");
  q.addBindValue(id);
  if (!q.exec() || !q.next()) return {};
  QJsonObject root;
  root["schema"] = "gesier.audit.requirement.v1";
  QJsonObject fields;
  const QStringList names{"code","title","description","typeId","type",
                          "statusId","status","source","traceRoot","comments",
                          "notApplicable"};
  for (int i = 0; i < names.size(); ++i) fields[names[i]] = value(q.value(i));
  root["fields"] = fields;
  root["productTrees"] = rows(db,
      "SELECT P.ID,P.NAME,RP.IS_PRIMARY FROM REQUIREMENT_PT RP JOIN PT P ON "
      "P.ID=RP.PT_ID WHERE RP.REQ_ID=? ORDER BY P.NAME,P.ID", id,
      {"id","label","primary"});
  root["applicability"] = rows(db,
      "SELECT C.ID,C.CODE,A.PT_ID,COALESCE(P.NAME,''),A.APPLICABLE,"
      "COALESCE(A.COMMENT,'') FROM REQUIREMENT_APPLICABILITY A JOIN "
      "CONFIGURATION C ON C.ID=A.CONFIG_ID LEFT JOIN PT P ON P.ID=A.PT_ID "
      "WHERE A.REQ_ID=? ORDER BY C.CODE,P.NAME", id,
      {"configurationId","configuration","ptId","productTree","applicable","comment"});
  root["verification"] = rows(db,
      "SELECT M.ID,M.METHOD,V.VERIFICATION_LEVEL_PT_ID,COALESCE(P.NAME,''),"
      "COALESCE(V.VERIF_LEVEL,''),COALESCE(V.PROCEDURE_REF,''),"
      "COALESCE(V.REDMINE_REF,''),COALESCE(V.MEANS,''),COALESCE(V.VERDICT,''),"
      "COALESCE(V.COMMENT,'') FROM REQUIREMENT_VERIFICATION V JOIN REQ_METHOD M "
      "ON M.ID=V.METHOD_ID LEFT JOIN PT P ON P.ID=V.VERIFICATION_LEVEL_PT_ID "
      "WHERE V.REQ_ID=? ORDER BY V.POSITION,M.METHOD", id,
      {"methodId","method","levelPtId","levelPt","level","procedure","redmine","means","verdict","comment"});
  QJsonArray relations;
  QSqlQuery rel(db);
  rel.prepare("SELECT L.ID,CASE WHEN L.SOURCE_REQ_ID=? THEN 'OUT' ELSE 'IN' END,T.CODE,"
              "CASE WHEN L.SOURCE_REQ_ID=? THEN B.ID ELSE A.ID END,CASE WHEN L.SOURCE_REQ_ID=? "
              "THEN B.CODE ELSE A.CODE END,COALESCE(L.COMMENT,'') FROM REQUIREMENT_RELATION L "
              "JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=L.TYPE_ID JOIN REQUIREMENT A ON "
              "A.ID=L.SOURCE_REQ_ID JOIN REQUIREMENT B ON B.ID=L.TARGET_REQ_ID WHERE "
              "L.SOURCE_REQ_ID=? OR L.TARGET_REQ_ID=? ORDER BY 2,3,5");
  for (int i=0;i<5;++i) rel.addBindValue(id);
  if (rel.exec()) while (rel.next()) { QJsonObject o; const QStringList k{"id","direction","type","otherId","other","comment"}; for(int i=0;i<k.size();++i)o[k[i]]=value(rel.value(i)); relations.append(o); }
  root["relations"] = relations;
  root["documents"] = rows(db,
      "SELECT DISTINCT D.ID,COALESCE((SELECT DR.REFERENCE FROM DOCUMENT_REFERENCE DR "
      "WHERE DR.DOC_ID=D.ID AND DR.IS_PRIMARY=1),D.TITLE),D.TITLE FROM DOCUMENT_NODE N "
      "JOIN DOCUMENT D ON D.ID=N.DOC_ID WHERE N.REQ_ID=? ORDER BY 2", id,
      {"id","reference","title"});
  root["interfaces"] = rows(db,
      "SELECT I.ID,COALESCE(I.CODE,CAST(I.ID AS TEXT)) FROM INTERFACE_REQUIREMENT X "
      "JOIN INTERFACE I ON I.ID=X.INTERFACE_ID WHERE X.REQ_ID=? ORDER BY 2", id,
      {"id","code"});
  root["changes"] = rows(db,
      "SELECT C.ID,C.CODE FROM CHANGE_LINK L JOIN CHANGE_ITEM C ON C.ID=L.CHANGE_ID "
      "WHERE L.OBJECT_TYPE='REQUIREMENT' AND L.OBJECT_ID=? ORDER BY C.CODE", id,
      {"id","code"});
  QJsonArray images;
  QSqlQuery image(db);
  image.prepare("SELECT TOKEN,COALESCE(MIME_TYPE,''),COALESCE(LEGEND,''),IMAGE FROM REQ_IMAGE WHERE REQ_ID=? ORDER BY TOKEN");
  image.addBindValue(id);
  if (image.exec()) while (image.next()) { const QByteArray data=image.value(3).toByteArray(); QJsonObject o; o["token"]=image.value(0).toString(); o["mimeType"]=image.value(1).toString(); o["legend"]=image.value(2).toString(); o["size"]=data.size(); o["sha256"]=QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()); images.append(o); }
  root["images"] = images;
  return root;
}

QList<AuditSubject> AuditService::requirementSubjects(QSqlDatabase db, int id) {
  QList<AuditSubject> out; QSet<QString> seen;
  addSubject(out, seen, "REQUIREMENT", id, "PRIMARY");
  auto collect=[&](const QString&type,const QString&sql){QSqlQuery q(db);q.prepare(sql);q.addBindValue(id);if(q.exec())while(q.next())addSubject(out,seen,type,q.value(0).toInt());};
  collect("PT","SELECT PT_ID FROM REQUIREMENT_PT WHERE REQ_ID=?");
  collect("CONFIGURATION","SELECT DISTINCT CONFIG_ID FROM REQUIREMENT_APPLICABILITY WHERE REQ_ID=?");
  collect("DOCUMENT","SELECT DISTINCT DOC_ID FROM DOCUMENT_NODE WHERE REQ_ID=?");
  collect("INTERFACE","SELECT INTERFACE_ID FROM INTERFACE_REQUIREMENT WHERE REQ_ID=?");
  collect("CHANGE","SELECT CHANGE_ID FROM CHANGE_LINK WHERE OBJECT_TYPE='REQUIREMENT' AND OBJECT_ID=?");
  QSqlQuery r(db);r.prepare("SELECT SOURCE_REQ_ID,TARGET_REQ_ID FROM REQUIREMENT_RELATION WHERE SOURCE_REQ_ID=? OR TARGET_REQ_ID=?");r.addBindValue(id);r.addBindValue(id);if(r.exec())while(r.next()){addSubject(out,seen,"REQUIREMENT",r.value(0).toInt());addSubject(out,seen,"REQUIREMENT",r.value(1).toInt());}
  return out;
}

bool AuditService::record(QSqlDatabase db, const QString &eventType,
                          const QString &objectType, int objectId,
                          const QJsonObject &before, const QJsonObject &after,
                          const QList<AuditSubject> &subjects,
                          const QString &comment, QString *error) {
  const QByteArray b=QJsonDocument(before).toJson(QJsonDocument::Compact);
  const QByteArray a=QJsonDocument(after).toJson(QJsonDocument::Compact);
  if (before==after) return true;
  const QString group=QUuid::createUuid().toString(QUuid::WithoutBraces);
  QSqlQuery q(db);q.prepare("INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON,COMMENT,EVENT_GROUP,PAYLOAD_VERSION) VALUES(?,?,?,?,?,?,?,2)");
  q.addBindValue(eventType);q.addBindValue(objectType);q.addBindValue(objectId);
  q.addBindValue(before.isEmpty()?QVariant():QVariant(QString::fromUtf8(b)));
  q.addBindValue(after.isEmpty()?QVariant():QVariant(QString::fromUtf8(a)));
  q.addBindValue(comment);q.addBindValue(group);
  if(!q.exec()){if(error)*error=q.lastError().text();return false;}
  const int eventId=q.lastInsertId().toInt();
  QList<AuditSubject> all=subjects; if(all.isEmpty())all.append({objectType,objectId,"PRIMARY"});
  QSet<QString> seen;
  for(const auto&s:all){if(s.objectId<0)continue;const QString key=s.objectType+':'+QString::number(s.objectId);if(seen.contains(key))continue;seen.insert(key);QSqlQuery x(db);x.prepare("INSERT INTO EVENT_SUBJECT(EVENT_ID,OBJECT_TYPE,OBJECT_ID,ROLE) VALUES(?,?,?,?) ON CONFLICT(EVENT_ID,OBJECT_TYPE,OBJECT_ID) DO UPDATE SET ROLE=excluded.ROLE");x.addBindValue(eventId);x.addBindValue(s.objectType);x.addBindValue(s.objectId);x.addBindValue(s.role);if(!x.exec()){if(error)*error=x.lastError().text();return false;}}
  return true;
}
