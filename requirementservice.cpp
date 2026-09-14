#include "requirementservice.h"
#include "producttreeservice.h"

#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
void addTextFilter(QStringList &where, QList<QVariant> &values,
                   const QString &column, const QString &value) {
  if (value.trimmed().isEmpty())
    return;
  where << QString("COALESCE(%1,'') LIKE ?").arg(column);
  values << "%" + value.trimmed() + "%";
}
void addIdFilter(QStringList &where, QList<QVariant> &values,
                 const QString &expression, const QList<int> &ids) {
  if (ids.isEmpty())
    return;
  where << QString("%1 IN (%2)")
               .arg(expression, QStringList(ids.size(), "?").join(','));
  for (int id : ids)
    values << id;
}
} // namespace

RequirementService::RequirementService(QString name)
    : m_connectionName(std::move(name)) {}

void RequirementService::setConnectionName(const QString &name) {
  m_connectionName = name;
}

QList<RequirementRecord>
RequirementService::find(const RequirementFilter &filter,
                         QString *error) const {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QStringList where{"1=1"};
  QList<QVariant> values;
  addTextFilter(where, values, "R.CODE", filter.code);
  addTextFilter(where, values, "R.TITLE", filter.title);
  addTextFilter(where, values, "R.DESCRIPTION", filter.description);
  addTextFilter(where, values, "R.SOURCE", filter.source);
  addIdFilter(where, values, "R.STATUS", filter.statusIds);
  addIdFilter(where, values, "R.TYPE", filter.typeIds);
  if (!filter.includeObsolete)
    where << "NOT EXISTS(SELECT 1 FROM REQ_STATUS S WHERE S.ID=R.STATUS AND "
             "(UPPER(S.STATUS)='OBSOLETE' OR UPPER(S.SHORTCUT)='O'))";
  if (!filter.ptIds.isEmpty()) {
    QStringList roots(filter.ptIds.size(), "?");
    where << "EXISTS(WITH RECURSIVE D(ID) AS(SELECT ID FROM PT WHERE ID IN (" +
                 roots.join(',') +
                 ") UNION ALL SELECT P.ID FROM PT P JOIN D ON P.PARENT=D.ID) "
                 "SELECT 1 FROM REQUIREMENT_PT X WHERE X.REQ_ID=R.ID AND "
                 "X.PT_ID IN(SELECT ID FROM D))";
    for (int id : filter.ptIds)
      values << id;
  }
  if (!filter.methodIds.isEmpty()) {
    where << "EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V WHERE "
             "V.REQ_ID=R.ID AND V.METHOD_ID IN (" +
                 QStringList(filter.methodIds.size(), "?").join(',') + "))";
    for (int id : filter.methodIds)
      values << id;
  }
  if (!filter.configurationIds.isEmpty()) {
    where << "EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A WHERE "
             "A.REQ_ID=R.ID AND A.CONFIG_ID IN (" +
                 QStringList(filter.configurationIds.size(), "?").join(',') +
                 "))";
    for (int id : filter.configurationIds)
      values << id;
  }
  auto coverage = [&](int requested, const QString &expression) {
    if (requested >= 0)
      where << (requested ? expression : "NOT (" + expression + ")");
  };
  coverage(filter.allocated,
           "EXISTS(SELECT 1 FROM REQUIREMENT_PT X WHERE X.REQ_ID=R.ID)");
  coverage(filter.traced, "EXISTS(SELECT 1 FROM REQUIREMENT_RELATION X WHERE "
                          "X.TARGET_REQ_ID=R.ID AND X.TYPE_ID IN(1,2))");
  coverage(filter.documented,
           "EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE N.REQ_ID=R.ID)");
  coverage(filter.verified,
           "EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V WHERE "
           "V.REQ_ID=R.ID AND V.METHOD_ID IS NOT NULL AND "
           "V.VERIFICATION_LEVEL_PT_ID IS NOT NULL)");

  QSqlQuery query(db);
  query.prepare(
      "SELECT R.ID,R.CODE,R.TITLE,R.DESCRIPTION,R.SOURCE,R.TYPE,R.STATUS,"
      "(SELECT PT_ID FROM REQUIREMENT_PT X WHERE X.REQ_ID=R.ID AND "
      "X.IS_PRIMARY=1),"
      "COALESCE((SELECT GROUP_CONCAT(NAME,' | ') FROM (SELECT T.NAME FROM "
      "REQUIREMENT_PT X JOIN PT T ON T.ID=X.PT_ID WHERE X.REQ_ID=R.ID ORDER BY "
      "T.POSITION,T.SEGMENT)),''),"
      "COALESCE((SELECT GROUP_CONCAT(METHOD,' | ') FROM (SELECT M.METHOD FROM "
      "REQUIREMENT_VERIFICATION V JOIN REQ_METHOD M ON M.ID=V.METHOD_ID WHERE "
      "V.REQ_ID=R.ID ORDER BY V.POSITION,V.ID)),''),"
      "COALESCE((SELECT GROUP_CONCAT(CODE,' | ') FROM (SELECT C.CODE FROM "
      "REQUIREMENT_APPLICABILITY A JOIN CONFIGURATION C ON C.ID=A.CONFIG_ID "
      "WHERE A.REQ_ID=R.ID AND A.PT_ID IS NULL ORDER BY C.CODE)),''),"
      "EXISTS(SELECT 1 FROM REQUIREMENT_PT X WHERE X.REQ_ID=R.ID),"
      "EXISTS(SELECT 1 FROM REQUIREMENT_RELATION X WHERE X.TARGET_REQ_ID=R.ID "
      "AND X.TYPE_ID IN(1,2)),"
      "EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE N.REQ_ID=R.ID),"
      "EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V WHERE V.REQ_ID=R.ID AND "
      "V.METHOD_ID IS NOT NULL AND V.VERIFICATION_LEVEL_PT_ID IS NOT NULL) "
      "FROM REQUIREMENT R WHERE " +
      where.join(" AND ") + " ORDER BY R.CODE");
  for (const QVariant &value : values)
    query.addBindValue(value);
  QList<RequirementRecord> result;
  if (!query.exec()) {
    if (error)
      *error = query.lastError().text();
    return result;
  }
  while (query.next()) {
    RequirementRecord r;
    r.id = query.value(0).toInt();
    r.code = query.value(1).toString();
    r.title = query.value(2).toString();
    r.description = query.value(3).toString();
    r.source = query.value(4).toString();
    r.typeId = query.value(5).toInt();
    r.statusId = query.value(6).toInt();
    r.primaryPtId = query.value(7).isNull() ? -1 : query.value(7).toInt();
    r.productTrees = query.value(8).toString();
    r.verificationMethods = query.value(9).toString();
    r.applicability = query.value(10).toString();
    r.allocated = query.value(11).toBool();
    r.traced = query.value(12).toBool();
    r.documented = query.value(13).toBool();
    r.verified = query.value(14).toBool();
    result << r;
  }
  return result;
}

RequirementRecord RequirementService::get(int id, QString *error) const {
  RequirementFilter filter;
  filter.includeObsolete = true;
  QList<RequirementRecord> records = find(filter, error);
  for (RequirementRecord r : records) {
    if (r.id != id)
      continue;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery pt(db);
    pt.prepare("SELECT PT_ID FROM REQUIREMENT_PT WHERE REQ_ID=? ORDER BY "
               "IS_PRIMARY DESC,PT_ID");
    pt.addBindValue(id);
    if (pt.exec())
      while (pt.next())
        r.ptIds << pt.value(0).toInt();
    QSqlQuery applicability(db);
    applicability.prepare("SELECT CONFIG_ID FROM REQUIREMENT_APPLICABILITY "
                          "WHERE REQ_ID=? AND PT_ID IS NULL ORDER BY CONFIG_ID");
    applicability.addBindValue(id);
    if (!applicability.exec()) {
      if (error)
        *error = applicability.lastError().text();
      return {};
    }
    while (applicability.next())
      r.configurationIds << applicability.value(0).toInt();
    QSqlQuery verification(db);
    verification.prepare(
        "SELECT "
        "ID,METHOD_ID,VERIFICATION_LEVEL_PT_ID,VERIF_LEVEL,PROCEDURE_REF,"
        "REDMINE_REF,MEANS,VERDICT,COMMENT,POSITION FROM "
        "REQUIREMENT_VERIFICATION WHERE REQ_ID=? ORDER "
        "BY POSITION,ID");
    verification.addBindValue(id);
    if (!verification.exec()) {
      if (error)
        *error = verification.lastError().text();
      return {};
    }
    while (verification.next()) {
      RequirementVerification v;
      v.id = verification.value(0).toInt();
      v.methodId = verification.value(1).toInt();
      v.levelPtId = verification.value(2).isNull()
                        ? -1
                        : verification.value(2).toInt();
      v.level = verification.value(3).toString();
      v.procedure = verification.value(4).toString();
      v.redmine = verification.value(5).toString();
      v.means = verification.value(6).toString();
      v.verdict = verification.value(7).toString();
      v.comment = verification.value(8).toString();
      v.position = verification.value(9).toInt();
      r.verifications << v;
    }
    return r;
  }
  return {};
}

QString RequirementService::suggestCode(int ptId) const {
  if (ptId < 0)
    return {};
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  ProductTreeService pt(m_connectionName);
  int number = 1;
  QSqlQuery sequence(db);
  sequence.prepare(
      "SELECT NEXT_VALUE FROM REQUIREMENT_CODE_SEQUENCE WHERE PT_ID=?");
  sequence.addBindValue(ptId);
  if (sequence.exec() && sequence.next())
    number = sequence.value(0).toInt();
  while (number < 100000) {
    QString code = QString("%1-R-%2")
                       .arg(pt.fullCode(ptId))
                       .arg(number++, 4, 10, QChar('0'));
    QSqlQuery used(db);
    used.prepare("SELECT 1 FROM REQUIREMENT WHERE CODE=?");
    used.addBindValue(code);
    if (used.exec() && !used.next())
      return code;
  }
  return {};
}

RequirementResult RequirementService::save(const RequirementRecord &record,
                                           bool manageTransaction) {
  if (record.code.trimmed().isEmpty() || record.title.trimmed().isEmpty())
    return RequirementResult::failure("Le code et le titre sont obligatoires.");
  if (record.primaryPtId < 0 || !record.ptIds.contains(record.primaryPtId))
    return RequirementResult::failure("Le Product Tree principal doit faire "
                                      "partie des allocations sélectionnées.");
  QSet<int> methods;
  for (const RequirementVerification &v : record.verifications) {
    if (v.methodId < 0)
      return RequirementResult::failure(
          "Chaque ligne de vérification doit avoir une méthode.");
    if (methods.contains(v.methodId))
      return RequirementResult::failure(
          "Une méthode de vérification ne peut être ajoutée qu'une fois.");
    if (!v.verdict.isEmpty() &&
        !QStringList{"C", "PC", "NC"}.contains(v.verdict))
      return RequirementResult::failure("Verdict de vérification invalide.");
    methods.insert(v.methodId);
  }
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  if (manageTransaction && !db.transaction())
    return RequirementResult::failure(db.lastError().text());
  int id = record.id;
  const RequirementVerification *first =
      record.verifications.isEmpty() ? nullptr : &record.verifications.first();
  ProductTreeService pt(m_connectionName);
  const QString firstLevel = first && first->levelPtId >= 0
                                 ? pt.fullCode(first->levelPtId)
                                 : (first ? first->level : QString());
  if (id < 0) {
    QSqlQuery documentCount(db);
    if (!documentCount.exec("SELECT 1 FROM DOCUMENT LIMIT 1")) {
      if (manageTransaction)
        db.rollback();
      return RequirementResult::failure(documentCount.lastError().text());
    }
    if (!documentCount.next()) {
      QSqlQuery compatibilityDocument(db);
      compatibilityDocument.prepare(
          "INSERT INTO DOCUMENT(PT_ID,TYPE,TITLE,DESCRIPTION) "
          "VALUES(?,COALESCE((SELECT ID FROM DOC_TYPE ORDER BY ID LIMIT 1),1),"
          "'Exigences non affectées à un document',"
          "'Conteneur technique de compatibilité ; ne constitue pas une "
          "couverture documentaire.')");
      compatibilityDocument.addBindValue(record.primaryPtId);
      if (!compatibilityDocument.exec()) {
        if (manageTransaction)
          db.rollback();
        return RequirementResult::failure(
            compatibilityDocument.lastError().text());
      }
    }
    QSqlQuery defaults(db);
    if (!defaults.exec(
            "SELECT (SELECT ID FROM DOCUMENT LIMIT 1),(SELECT ID FROM REQ_TYPE "
            "ORDER BY ID LIMIT 1),(SELECT ID FROM REQ_STATUS ORDER BY ID LIMIT "
            "1),(SELECT ID FROM REQ_METHOD ORDER BY ID LIMIT 1)") ||
        !defaults.next() || defaults.value(0).isNull()) {
      if (manageTransaction) db.rollback();
      return RequirementResult::failure(
          "Les catalogues nécessaires à la création sont incomplets.");
    }
    QSqlQuery insert(db);
    insert.prepare(
        "INSERT INTO "
        "REQUIREMENT(PT_ID,CODE,DOC_ID,TITLE,DESCRIPTION,TYPE,STATUS,SOURCE,"
        "VERIF_LEVEL,VERIF_METHOD,VERIF_PROCEDURE,REDMINE_REF,VERIF_MEANS,"
        "VERIF_STATUS) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    insert.addBindValue(record.primaryPtId);
    insert.addBindValue(record.code.trimmed());
    insert.addBindValue(defaults.value(0));
    insert.addBindValue(record.title.trimmed());
    insert.addBindValue(record.description);
    insert.addBindValue(record.typeId < 0 ? defaults.value(1) : record.typeId);
    insert.addBindValue(record.statusId < 0 ? defaults.value(2)
                                            : record.statusId);
    insert.addBindValue(record.source);
    insert.addBindValue(first ? QVariant(firstLevel) : QVariant());
    insert.addBindValue(first ? QVariant(first->methodId) : defaults.value(3));
    insert.addBindValue(first ? first->procedure : QString());
    insert.addBindValue(first ? first->redmine : QString());
    insert.addBindValue(first ? first->means : QString());
    insert.addBindValue(first ? first->verdict : QString());
    if (!insert.exec()) {
      if (manageTransaction) db.rollback();
      return RequirementResult::failure(insert.lastError().text());
    }
    id = insert.lastInsertId().toInt();
  } else {
    QSqlQuery update(db);
    update.prepare("UPDATE REQUIREMENT SET "
                   "PT_ID=?,CODE=?,TITLE=?,DESCRIPTION=?,TYPE=?,STATUS=?,"
                   "SOURCE=?,VERIF_LEVEL=?,VERIF_METHOD=COALESCE(?,VERIF_"
                   "METHOD),VERIF_PROCEDURE=?,"
                   "REDMINE_REF=?,VERIF_MEANS=?,VERIF_STATUS=? WHERE ID=?");
    update.addBindValue(record.primaryPtId);
    update.addBindValue(record.code.trimmed());
    update.addBindValue(record.title.trimmed());
    update.addBindValue(record.description);
    update.addBindValue(record.typeId);
    update.addBindValue(record.statusId);
    update.addBindValue(record.source);
    update.addBindValue(firstLevel);
    update.addBindValue(first ? QVariant(first->methodId) : QVariant());
    update.addBindValue(first ? first->procedure : QString());
    update.addBindValue(first ? first->redmine : QString());
    update.addBindValue(first ? first->means : QString());
    update.addBindValue(first ? first->verdict : QString());
    update.addBindValue(id);
    if (!update.exec()) {
      if (manageTransaction) db.rollback();
      return RequirementResult::failure(update.lastError().text());
    }
  }
  QSqlQuery clearPt(db);
  clearPt.prepare("DELETE FROM REQUIREMENT_PT WHERE REQ_ID=?");
  clearPt.addBindValue(id);
  if (!clearPt.exec()) {
    if (manageTransaction) db.rollback();
    return RequirementResult::failure(clearPt.lastError().text());
  }
  for (int ptId : record.ptIds) {
    QSqlQuery allocation(db);
    allocation.prepare(
        "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(?,?,?)");
    allocation.addBindValue(id);
    allocation.addBindValue(ptId);
    allocation.addBindValue(ptId == record.primaryPtId);
    if (!allocation.exec()) {
      if (manageTransaction) db.rollback();
      return RequirementResult::failure(allocation.lastError().text());
    }
  }
  QSqlQuery clearApplicability(db);
  // The requirement editor owns only the simple applicability. PT-specific
  // overrides are managed by the matrix and must survive ordinary edits.
  clearApplicability.prepare("DELETE FROM REQUIREMENT_APPLICABILITY WHERE REQ_ID=? AND PT_ID IS NULL");
  clearApplicability.addBindValue(id);
  if (!clearApplicability.exec()) {
    if (manageTransaction) db.rollback();
    return RequirementResult::failure(clearApplicability.lastError().text());
  }
  for (int configurationId : record.configurationIds) {
    QSqlQuery applicability(db);
    applicability.prepare("INSERT INTO REQUIREMENT_APPLICABILITY(REQ_ID,CONFIG_ID,PT_ID,APPLICABLE) VALUES(?,?,NULL,1)");
    applicability.addBindValue(id);
    applicability.addBindValue(configurationId);
    if (!applicability.exec()) {
      if (manageTransaction) db.rollback();
      return RequirementResult::failure(applicability.lastError().text());
    }
  }
  QSqlQuery clearVerification(db);
  clearVerification.prepare(
      "DELETE FROM REQUIREMENT_VERIFICATION WHERE REQ_ID=?");
  clearVerification.addBindValue(id);
  if (!clearVerification.exec()) {
    if (manageTransaction) db.rollback();
    return RequirementResult::failure(clearVerification.lastError().text());
  }
  for (int position = 0; position < record.verifications.size(); ++position) {
    const auto &v = record.verifications[position];
    QSqlQuery insert(db);
    insert.prepare("INSERT INTO "
                   "REQUIREMENT_VERIFICATION(REQ_ID,METHOD_ID,VERIFICATION_LEVEL_PT_ID,VERIF_LEVEL,"
                   "PROCEDURE_REF,REDMINE_REF,MEANS,VERDICT,COMMENT,POSITION) "
                   "VALUES(?,?,?,?,?,?,?,?,?,?)");
    insert.addBindValue(id);
    insert.addBindValue(v.methodId);
    insert.addBindValue(v.levelPtId < 0 ? QVariant() : QVariant(v.levelPtId));
    insert.addBindValue(v.level);
    insert.addBindValue(v.procedure);
    insert.addBindValue(v.redmine);
    insert.addBindValue(v.means);
    insert.addBindValue(v.verdict.isEmpty() ? QVariant() : QVariant(v.verdict));
    insert.addBindValue(v.comment);
    insert.addBindValue(position);
    if (!insert.exec()) {
      if (manageTransaction) db.rollback();
      return RequirementResult::failure(insert.lastError().text());
    }
  }
  QRegularExpression automatic(
      "^" + QRegularExpression::escape(pt.fullCode(record.primaryPtId)) +
      "-R-(\\d{4,})$");
  auto match = automatic.match(record.code.trimmed());
  if (match.hasMatch()) {
    QSqlQuery sequence(db);
    sequence.prepare("INSERT INTO REQUIREMENT_CODE_SEQUENCE(PT_ID,NEXT_VALUE) "
                     "VALUES(?,?) ON CONFLICT(PT_ID) DO UPDATE SET "
                     "NEXT_VALUE=MAX(NEXT_VALUE,excluded.NEXT_VALUE)");
    sequence.addBindValue(record.primaryPtId);
    sequence.addBindValue(match.captured(1).toInt() + 1);
    if (!sequence.exec()) {
      if (manageTransaction) db.rollback();
      return RequirementResult::failure(sequence.lastError().text());
    }
  }
  QSqlQuery log(db);
  log.prepare(
      "INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
      "VALUES(?,'REQUIREMENT',?,?)");
  log.addBindValue(record.id < 0 ? "CREATE" : "UPDATE");
  log.addBindValue(id);
  log.addBindValue(record.code);
  if (!log.exec()) {
    const QString message = log.lastError().text();
    if (manageTransaction) db.rollback();
    return RequirementResult::failure(message);
  }
  if (manageTransaction && !db.commit()) {
    const QString message = db.lastError().text();
    db.rollback();
    return RequirementResult::failure(message);
  }
  return RequirementResult::successResult("Exigence enregistrée.", id,
                                          record.code);
}

RequirementResult RequirementService::setObsolete(int id) {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  QSqlQuery status(db);
  if (!status.exec("SELECT ID FROM REQ_STATUS WHERE UPPER(STATUS)='OBSOLETE' "
                   "OR UPPER(SHORTCUT)='O' LIMIT 1") ||
      !status.next())
    return RequirementResult::failure("Aucun statut Obsolète n'est configuré.");
  QSqlQuery update(db);
  update.prepare("UPDATE REQUIREMENT SET STATUS=? WHERE ID=?");
  update.addBindValue(status.value(0));
  update.addBindValue(id);
  if (!update.exec())
    return RequirementResult::failure(update.lastError().text());
  return RequirementResult::successResult("Exigence rendue obsolète.", id);
}
