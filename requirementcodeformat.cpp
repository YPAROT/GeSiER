#include "requirementcodeformat.h"

#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString meta(QSqlDatabase db, const QString &key, const QString &fallback) {
  QSqlQuery query(db);
  query.prepare("SELECT VALUE FROM PROJECT_META WHERE KEY=?");
  query.addBindValue(key);
  return query.exec() && query.next() && !query.value(0).toString().isEmpty()
             ? query.value(0).toString()
             : fallback;
}

QStringList segments(QSqlDatabase db, int id) {
  QSqlQuery query(db);
  query.prepare(
      "WITH RECURSIVE P(ID,SEGMENT,PARENT,L) AS(SELECT ID,SEGMENT,PARENT,0 "
      "FROM PT WHERE ID=? UNION ALL SELECT X.ID,X.SEGMENT,X.PARENT,P.L+1 FROM "
      "PT X JOIN P ON X.ID=P.PARENT) SELECT SEGMENT FROM P ORDER BY L DESC");
  query.addBindValue(id);
  if (!query.exec()) return {};
  QStringList result;
  while (query.next()) result << query.value(0).toString();
  return result;
}
}

RequirementCodeFormatService::RequirementCodeFormatService(QString connectionName)
    : m_connectionName(connectionName) {}

void RequirementCodeFormatService::setConnectionName(const QString &connectionName) {
  m_connectionName = connectionName;
}

RequirementCodeFormat RequirementCodeFormatService::format() const {
  RequirementCodeFormat result;
  const QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
  if (!db.isValid() || !db.isOpen()) return result;
  const QString separator = meta(db, "pt_separator", "-");
  result.separator = separator.isEmpty() ? QChar('-') : separator.front();
  result.marker = meta(db, "requirement_code_marker", "R");
  bool ok = false;
  const int digits = meta(db, "requirement_code_digits", "4").toInt(&ok);
  result.digits = ok && digits >= 1 && digits <= 9 ? digits : 4;
  result.includeRoot = meta(db, "requirement_code_include_root", "1") != "0";
  return result;
}

RequirementCodeFormatResult RequirementCodeFormatService::saveFormat(
    const RequirementCodeFormat &value) const {
  if (value.separator.isNull() || value.separator.isSpace() ||
      value.separator.isLetterOrNumber())
    return RequirementCodeFormatResult::failure(
        "Le séparateur doit être un caractère visible non alphanumérique.");
  if (!QRegularExpression("^[A-Z]{1,5}$").match(value.marker).hasMatch())
    return RequirementCodeFormatResult::failure(
        "Le marqueur doit contenir de 1 à 5 lettres majuscules.");
  if (value.digits < 1 || value.digits > 9)
    return RequirementCodeFormatResult::failure(
        "Le nombre de chiffres doit être compris entre 1 et 9.");
  QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
  if (!db.isValid() || !db.isOpen())
    return RequirementCodeFormatResult::failure("La base du projet n'est pas ouverte.");
  if (!db.transaction()) return RequirementCodeFormatResult::failure(db.lastError().text());
  const QList<QPair<QString, QString>> values{
      {"pt_separator", QString(value.separator)},
      {"requirement_code_marker", value.marker},
      {"requirement_code_digits", QString::number(value.digits)},
      {"requirement_code_include_root", value.includeRoot ? "1" : "0"}};
  for (const auto &entry : values) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO PROJECT_META(KEY,VALUE) VALUES(?,?) ON CONFLICT(KEY) "
                  "DO UPDATE SET VALUE=excluded.VALUE");
    query.addBindValue(entry.first);
    query.addBindValue(entry.second);
    if (!query.exec()) {
      const QString error = query.lastError().text(); db.rollback();
      return RequirementCodeFormatResult::failure(error);
    }
  }
  QSqlQuery ids("SELECT ID FROM PT", db);
  if (!ids.exec()) { const QString error=ids.lastError().text(); db.rollback(); return RequirementCodeFormatResult::failure(error); }
  QList<int> ptIds; while (ids.next()) ptIds << ids.value(0).toInt();
  for (int id : ptIds) {
    QSqlQuery update(db);
    update.prepare("UPDATE PT SET NAME=? WHERE ID=?");
    update.addBindValue(segments(db, id).join(value.separator));
    update.addBindValue(id);
    if (!update.exec()) { const QString error=update.lastError().text(); db.rollback(); return RequirementCodeFormatResult::failure(error); }
  }
  if (!db.commit()) return RequirementCodeFormatResult::failure(db.lastError().text());
  return RequirementCodeFormatResult::ok();
}

QString RequirementCodeFormatService::productTreePath(int ptId,
                                                       bool applyRootSetting) const {
  if (ptId < 0) return {};
  const RequirementCodeFormat value = format();
  QStringList path = segments(QSqlDatabase::database(m_connectionName), ptId);
  if (applyRootSetting && !value.includeRoot && !path.isEmpty()) path.removeFirst();
  return path.join(value.separator);
}

QString RequirementCodeFormatService::prefix(int ptId) const {
  const RequirementCodeFormat value = format();
  const QString path = productTreePath(ptId);
  return path.isEmpty() ? value.marker + value.separator
                        : path + value.separator + value.marker + value.separator;
}

QString RequirementCodeFormatService::code(int ptId, int index) const {
  const RequirementCodeFormat value = format();
  if (ptId < 0 || index < 0) return {};
  return prefix(ptId) + QString("%1").arg(index, value.digits, 10, QChar('0'));
}

RequirementCodeValidation RequirementCodeFormatService::validate(
    const QString &rawCode, int ptId) const {
  RequirementCodeValidation result;
  if (ptId < 0) { result.errors << "Aucun PT principal n'est sélectionné."; return result; }
  const RequirementCodeFormat value = format();
  const QString expectedPrefix = prefix(ptId);
  const QString code = rawCode.trimmed();
  if (!code.startsWith(expectedPrefix)) {
    const QString expectedPath = productTreePath(ptId);
    const QString fullPath = productTreePath(ptId, false);
    if (!value.includeRoot && fullPath != expectedPath &&
        code.startsWith(fullPath + value.separator))
      result.errors << "La racine du Product Tree ne doit pas être incluse.";
    if (!expectedPath.isEmpty() && !code.startsWith(expectedPath + value.separator))
      result.errors << QString("Le chemin du PT principal attendu est « %1 ».").arg(expectedPath);
    result.errors << QString("Le séparateur et le marqueur attendus sont « %1%2%1 ».")
                         .arg(value.separator).arg(value.marker);
  }
  const QString indexText = code.startsWith(expectedPrefix) ? code.mid(expectedPrefix.size()) : QString();
  if (indexText.isEmpty()) result.errors << "L'index est absent.";
  else if (!QRegularExpression("^[0-9]+$").match(indexText).hasMatch())
    result.errors << "L'index doit être numérique.";
  else {
    if (indexText.size() != value.digits)
      result.errors << QString("L'index doit comporter exactement %1 chiffre(s).").arg(value.digits);
    bool ok = false; const qlonglong index = indexText.toLongLong(&ok);
    if (!ok || index < 0) result.errors << "L'index doit être positif ou nul.";
    else if (index <= 2147483647) result.index = int(index);
  }
  result.conforming = result.errors.isEmpty();
  return result;
}
