#include "historyservice.h"

#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

HistoryService::HistoryService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

static QSqlDatabase historyDb(const QString &name) {
  return QSqlDatabase::contains(name)
             ? QSqlDatabase::database(name, false)
             : QSqlDatabase();
}

QList<HistoryRecord> HistoryService::find(const QString &text,
                                           const QString &objectType,
                                           const QString &eventType,
                                           int objectId) const {
  QList<HistoryRecord> result;
  QSqlQuery q(historyDb(m_connectionName));
  QString sql = "SELECT E.ID,E.EVENT_TIME,COALESCE(E.AUTHOR,''),E.EVENT_TYPE,"
                "E.OBJECT_TYPE,COALESCE(E.OBJECT_ID,-1),COALESCE(E.BEFORE_JSON,''),"
                "COALESCE(E.AFTER_JSON,''),COALESCE(E.COMMENT,''),"
                "COALESCE(E.EVENT_GROUP,''),COALESCE(E.PAYLOAD_VERSION,1) "
                "FROM EVENT_LOG E WHERE 1=1";
  QVariantList values;
  if (!objectType.isEmpty()) { sql += " AND EXISTS(SELECT 1 FROM EVENT_SUBJECT S WHERE S.EVENT_ID=E.ID AND S.OBJECT_TYPE=?)"; values << objectType; }
  if (!eventType.isEmpty()) { sql += " AND E.EVENT_TYPE=?"; values << eventType; }
  if (objectId >= 0) { sql += " AND EXISTS(SELECT 1 FROM EVENT_SUBJECT S WHERE S.EVENT_ID=E.ID AND S.OBJECT_ID=?"; values << objectId; if(!objectType.isEmpty()){sql+=" AND S.OBJECT_TYPE=?";values<<objectType;} sql+=")"; }
  if (!text.trimmed().isEmpty()) {
    sql += " AND (E.AUTHOR LIKE ? OR E.OBJECT_TYPE LIKE ? OR E.EVENT_TYPE LIKE ? OR "
           "E.BEFORE_JSON LIKE ? OR E.AFTER_JSON LIKE ? OR E.COMMENT LIKE ?)";
    const QString pattern = "%" + text.trimmed() + "%";
    for (int i = 0; i < 6; ++i) values << pattern;
  }
  sql += " ORDER BY E.ID DESC LIMIT 5000";
  q.prepare(sql);
  for (const QVariant &value : values) q.addBindValue(value);
  if (!q.exec()) return result;
  while (q.next()) {
    HistoryRecord r;
    r.id=q.value(0).toInt(); r.time=q.value(1).toString();
    r.author=q.value(2).toString(); r.eventType=q.value(3).toString();
    r.objectType=q.value(4).toString(); r.objectId=q.value(5).toInt();
    r.beforeJson=q.value(6).toString(); r.afterJson=q.value(7).toString();
    r.comment=q.value(8).toString(); r.groupId=q.value(9).toString();
    r.payloadVersion=q.value(10).toInt();
    r.diffHtml=formatDiff(r.beforeJson,r.afterJson,&r.partial,&r.summary);
    if(r.payloadVersion<2){r.partial=true;r.diffHtml="<i>Donnée historique partielle</i><br>"+r.diffHtml;r.summary="Donnée historique partielle\n"+r.summary;}
    QSqlQuery subjects(historyDb(m_connectionName));
    subjects.prepare("SELECT OBJECT_TYPE||':'||OBJECT_ID||CASE ROLE WHEN 'PRIMARY' THEN ' (principal)' ELSE '' END FROM EVENT_SUBJECT WHERE EVENT_ID=? ORDER BY ROLE DESC,OBJECT_TYPE,OBJECT_ID");
    subjects.addBindValue(r.id);
    if(subjects.exec())while(subjects.next())r.subjects<<subjects.value(0).toString();
    result << r;
  }
  return result;
}

namespace {
QString human(const QString &key) {
  static const QHash<QString,QString> labels{{"fields","Champs de l'exigence"},{"productTrees","Allocations Product Tree"},{"applicability","Applicabilité"},{"verification","Vérification"},{"relations","Relations et rattachements"},{"documents","Documents"},{"interfaces","Interfaces"},{"changes","Changements"},{"images","Images"},{"title","Titre"},{"description","Description"},{"status","Statut"},{"type","Type"},{"source","Source"},{"comments","Commentaires"},{"traceRoot","Racine de traçabilité"},{"notApplicable","Non applicable au projet"},{"code","Code"}};
  return labels.value(key,key);
}
QString display(const QJsonValue &v) {
  if(v.isNull()||v.isUndefined())return "∅";
  if(v.isBool())return v.toBool()?"Oui":"Non";
  if(v.isString())return v.toString().isEmpty()?"∅":v.toString();
  if(v.isDouble())return QString::number(v.toDouble(),'g',15);
  return QString::fromUtf8(QJsonDocument(v.isArray()?v.toArray():QJsonArray{v.toObject()}).toJson(QJsonDocument::Compact));
}
void compare(const QJsonValue &before,const QJsonValue &after,const QString &path,QStringList &html,QStringList &plain){
  if(before==after)return;
  if(before.isObject()&&after.isObject()){
    QSet<QString> keys;for(const auto&k:before.toObject().keys())keys<<k;for(const auto&k:after.toObject().keys())keys<<k;
    QStringList sorted=keys.values();sorted.sort();for(const auto&k:sorted){if(k=="schema"||k.endsWith("Id"))continue;compare(before.toObject().value(k),after.toObject().value(k),path.isEmpty()?human(k):path+" › "+human(k),html,plain);}return;
  }
  const QString label=path.isEmpty()?"Valeur":path;
  if(before.isArray()&&after.isArray()){
    QMap<QString,QString> oldItems,newItems;
    auto collect=[](const QJsonArray&a,QMap<QString,QString>&out){for(const auto&v:a){QString key,text;if(v.isObject()){const auto o=v.toObject();for(const QString candidate:{"token","code","reference","method","label","other","id"})if(o.contains(candidate)){key=candidate+":"+display(o.value(candidate));break;}text=QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));}else{text=display(v);key=text;}out.insert(key,text);}};
    collect(before.toArray(),oldItems);collect(after.toArray(),newItems);
    QSet<QString> keys;for(const auto&k:oldItems.keys())keys<<k;for(const auto&k:newItems.keys())keys<<k;QStringList sorted=keys.values();sorted.sort();
    for(const auto&k:sorted){if(oldItems.value(k)==newItems.value(k))continue;if(oldItems.contains(k)){html<<"<span style='color:#b42318'>− <b>"+label.toHtmlEscaped()+"</b> : "+oldItems.value(k).toHtmlEscaped()+"</span>";plain<<"− "+label+" : "+oldItems.value(k);}if(newItems.contains(k)){html<<"<span style='color:#067647'>+ <b>"+label.toHtmlEscaped()+"</b> : "+newItems.value(k).toHtmlEscaped()+"</span>";plain<<"+ "+label+" : "+newItems.value(k);}}
    return;
  }
  if(!before.isUndefined()&&!before.isNull()){const QString s=display(before);html<<"<span style='color:#b42318'>− <b>"+label.toHtmlEscaped()+"</b> : "+s.toHtmlEscaped()+"</span>";plain<<"− "+label+" : "+s;}
  if(!after.isUndefined()&&!after.isNull()){const QString s=display(after);html<<"<span style='color:#067647'>+ <b>"+label.toHtmlEscaped()+"</b> : "+s.toHtmlEscaped()+"</span>";plain<<"+ "+label+" : "+s;}
}
}

QString HistoryService::formatDiff(const QString &beforeJson,const QString &afterJson,bool *partial,QString *plainText){
  QJsonParseError be,ae;const auto bd=QJsonDocument::fromJson(beforeJson.toUtf8(),&be);const auto ad=QJsonDocument::fromJson(afterJson.toUtf8(),&ae);
  const bool bEmpty=beforeJson.trimmed().isEmpty(),aEmpty=afterJson.trimmed().isEmpty();
  const bool validB=bEmpty||be.error==QJsonParseError::NoError,validA=aEmpty||ae.error==QJsonParseError::NoError;
  QStringList html,plain;
  if(validB&&validA){compare(bEmpty?QJsonValue():QJsonValue(bd.object()),aEmpty?QJsonValue():QJsonValue(ad.object()),{},html,plain);}else{
    if(!bEmpty){html<<"<span style='color:#b42318'>− "+beforeJson.toHtmlEscaped()+"</span>";plain<<"− "+beforeJson;}
    if(!aEmpty){html<<"<span style='color:#067647'>+ "+afterJson.toHtmlEscaped()+"</span>";plain<<"+ "+afterJson;}
  }
  const bool isPartial=!validB||!validA;
  if(isPartial){html.prepend("<i>Donnée historique partielle</i>");plain.prepend("Donnée historique partielle");}
  if(partial) *partial=isPartial;
  if(plainText) *plainText=plain.join('\n');
  return html.join("<br>");
}

static QStringList distinctValues(const QString &connection,
                                  const QString &column) {
  QStringList values;
  QSqlQuery q("SELECT DISTINCT " + column + " FROM EVENT_LOG ORDER BY " + column,
              historyDb(connection));
  while (q.next()) values << q.value(0).toString();
  return values;
}

QStringList HistoryService::objectTypes() const {
  return distinctValues(m_connectionName, "OBJECT_TYPE");
}
QStringList HistoryService::eventTypes() const {
  return distinctValues(m_connectionName, "EVENT_TYPE");
}

ProjectDiagnostic HistoryService::diagnose() const {
  ProjectDiagnostic d;
  const QSqlDatabase db = historyDb(m_connectionName);
  if (!db.isValid() || !db.isOpen()) { d.errors << "Aucun projet ouvert."; return d; }
  d.fileSize = QFileInfo(db.databaseName()).size();
  QSqlQuery version("PRAGMA user_version", db);
  if (version.next()) d.schemaVersion = version.value(0).toInt();
  QSqlQuery quick("PRAGMA quick_check", db);
  while (quick.next()) if (quick.value(0).toString() != "ok") d.errors << quick.value(0).toString();
  QSqlQuery fk("PRAGMA foreign_key_check", db);
  while (fk.next()) d.errors << QString("Clé étrangère invalide : %1 ligne %2 vers %3")
      .arg(fk.value(0).toString(), fk.value(1).toString(), fk.value(2).toString());
  QSqlQuery integrity("PRAGMA integrity_check", db);
  while (integrity.next()) if (integrity.value(0).toString() != "ok") d.errors << integrity.value(0).toString();
  d.ok = d.errors.isEmpty();
  return d;
}

QString ProjectDiagnostic::summary() const {
  QString text = QString("Schéma : v%1\nTaille : %2 octets\nÉtat : %3")
                     .arg(schemaVersion).arg(fileSize).arg(ok ? "intègre" : "anomalies détectées");
  if (!errors.isEmpty()) text += "\n\nErreurs :\n- " + errors.join("\n- ");
  if (!warnings.isEmpty()) text += "\n\nAvertissements :\n- " + warnings.join("\n- ");
  return text;
}

bool HistoryService::setAuthor(const QString &author, QString *error) const {
  QSqlQuery q(historyDb(m_connectionName));
  q.prepare("UPDATE EVENT_CONTEXT SET AUTHOR=? WHERE ID=1");
  q.addBindValue(author.trimmed());
  if (q.exec()) return true;
  if (error) *error = q.lastError().text();
  return false;
}
