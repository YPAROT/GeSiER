#include "req_sqlmanager.h"
#include "databasemigrator.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QUuid>

REQ_SQLManager::REQ_SQLManager() : m_DBConnectionName("REQ_DB") {}

QSqlError REQ_SQLManager::newDB(QString filename) {
  if (filename.isEmpty())
    return error("Aucun nom de fichier n'a été fourni.");
  close();
  if (QFile::exists(filename) && !QFile::remove(filename))
    return error("Impossible de remplacer le fichier de projet existant.");
  QSqlDatabase initialDb =
      QSqlDatabase::addDatabase("QSQLITE", m_DBConnectionName);
  initialDb.setDatabaseName(filename);
  if (!initialDb.open()) {
    QSqlError openError = initialDb.lastError();
    initialDb = QSqlDatabase();
    close();
    return openError;
  }
  QSqlQuery initialPragma(initialDb);
  initialPragma.exec("PRAGMA foreign_keys = ON");

  QFile file(":/files/createTables.sql");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    initialPragma = QSqlQuery();
    initialDb = QSqlDatabase();
    close();
    QFile::remove(filename);
    return error("Impossible de lire le schéma de création embarqué.");
  }
  initialPragma = QSqlQuery();
  initialDb = QSqlDatabase();
  QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName);
  QSqlQuery query(db);
  const QStringList statements =
      QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
  for (const QString &raw : statements) {
    const QString sql = raw.trimmed();
    if (sql.isEmpty() || sql.startsWith("--"))
      continue;
    if (!query.exec(sql)) {
      const QString message = query.lastError().text();
      query = QSqlQuery();
      db = QSqlDatabase();
      close();
      QFile::remove(filename);
      return error("Création de la base interrompue : " + message);
    }
  }
  QString migrationError;
  if (!DatabaseMigrator::migrate(db, &migrationError)) {
    query = QSqlQuery();
    db = QSqlDatabase();
    close();
    QFile::remove(filename);
    return error("Initialisation du schéma interrompue : " + migrationError);
  }
  // Catalogues supplied only for brand-new projects. Existing projects keep
  // their customized values during DatabaseMigrator::migrate().
  QSqlQuery catalog(db);
  if (!catalog.exec("DELETE FROM REQ_TYPE") ||
      !catalog.exec(
          "INSERT INTO REQ_TYPE(ID,CODE,TYPE) VALUES "
          "(1,'PERF','Performances'),(2,'FUNC','Functional'),"
          "(3,'ENVI','Environment'),(4,'IF','Interfaces'),"
          "(5,'AIT','Assembly Integration and Tests'),(6,'DES','Design'),"
          "(7,'DEV','Development'),(8,'OP','Operational'),"
          "(9,'QA','Quality Assurance'),"
          "(10,'SAD','Safety, Availability and Dependability'),"
          "(11,'VERI','Verification'),(12,'INS','Inspection'),"
          "(13,'GSE','Ground Segment Equipment'),"
          "(14,'ILS','Integrated Logistics Support')")) {
    const QString message = catalog.lastError().text();
    catalog = QSqlQuery();
    query = QSqlQuery();
    db = QSqlDatabase();
    close();
    QFile::remove(filename);
    return error("Initialisation des types d'exigence interrompue : " +
                 message);
  }
  m_filename = filename;
  return QSqlError();
}

QSqlError REQ_SQLManager::openDB(QString filename, bool backupBeforeMigration) {
  close();
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_DBConnectionName);
  if (!db.isValid()) {
    db = QSqlDatabase();
    close();
    return error("Le pilote SQLite n'est pas disponible.");
  }
  db.setDatabaseName(filename);
  if (!db.open()) {
    QSqlError openError = db.lastError();
    db = QSqlDatabase();
    close();
    return openError;
  }
  QSqlQuery pragma(db);
  if (!pragma.exec("PRAGMA foreign_keys = ON")) {
    QSqlError pragmaError = pragma.lastError();
    pragma = QSqlQuery();
    db = QSqlDatabase();
    close();
    return pragmaError;
  }
  QSqlQuery versionQuery("PRAGMA user_version", db);
  const int schemaVersion =
      versionQuery.next() ? versionQuery.value(0).toInt() : 0;
  if (backupBeforeMigration &&
      schemaVersion < DatabaseMigrator::CurrentVersion &&
      QFileInfo(filename).size() > 0 && !createBackup(filename)) {
    versionQuery = QSqlQuery();
    pragma = QSqlQuery();
    db = QSqlDatabase();
    close();
    return error(
        "Impossible de créer la sauvegarde de sécurité avant ouverture.");
  }
  // La migration v12 reconstruit la table INTERFACE sur les anciens projets.
  // Toute requête encore active sur le schéma empêcherait alors le DROP TABLE
  // avec l'erreur SQLite "database table is locked".
  versionQuery = QSqlQuery();
  pragma = QSqlQuery();
  QString migrationError;
  if (!DatabaseMigrator::migrate(db, &migrationError)) {
    versionQuery = QSqlQuery();
    pragma = QSqlQuery();
    db = QSqlDatabase();
    close();
    return error("Migration du projet impossible : " + migrationError);
  }
  m_filename = filename;
  return QSqlError();
}

void REQ_SQLManager::close() {
  if (!QSqlDatabase::contains(m_DBConnectionName))
    return;
  {
    QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName);
    db.close();
  }
  QSqlDatabase::removeDatabase(m_DBConnectionName);
}

bool REQ_SQLManager::saveAs(QString filename) {
  if (m_filename.isEmpty() || filename.isEmpty())
    return false;
  const QString source = m_filename;
  const QString temporary = filename + ".gesier-copy-" +
                            QUuid::createUuid().toString(QUuid::WithoutBraces);
  close();
  bool copied = QFile::copy(source, temporary);
  if (copied && QFile::exists(filename))
    copied = QFile::remove(filename);
  if (copied)
    copied = QFile::rename(temporary, filename);
  if (!copied)
    QFile::remove(temporary);
  const QSqlError reopenError = openDB(copied ? filename : source, false);
  return copied && reopenError.type() == QSqlError::NoError;
}

QString REQ_SQLManager::currentConnection() const { return m_DBConnectionName; }

bool REQ_SQLManager::execQuery(QString queryStr) {
  if (!QSqlDatabase::contains(m_DBConnectionName)) {
    m_lastError = "Aucun projet n'est ouvert.";
    return false;
  }
  QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName, false);
  QSqlQuery query(db);
  if (!db.isOpen() || !query.exec(queryStr)) {
    m_lastError = query.lastError().text();
    return false;
  }
  return true;
}

QVector<QStringList> REQ_SQLManager::execQueryAndGetResults(QString queryStr) {
  QVector<QStringList> result;
  if (!QSqlDatabase::contains(m_DBConnectionName)) {
    m_lastError = "Aucun projet n'est ouvert.";
    return result;
  }
  QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName, false);
  QSqlQuery query(db);
  if (!db.isOpen() || !query.exec(queryStr)) {
    m_lastError = query.lastError().text();
    return result;
  }
  while (query.next()) {
    QStringList row;
    for (int c = 0; c < query.record().count(); ++c)
      row << query.value(c).toString();
    result << row;
  }
  return result;
}

QStringList REQ_SQLManager::getColumnFromQuery(QString queryStr, int colnum) {
  QStringList result;
  if (!QSqlDatabase::contains(m_DBConnectionName)) {
    m_lastError = "Aucun projet n'est ouvert.";
    return result;
  }
  QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName, false);
  QSqlQuery query(db);
  if (!db.isOpen() || !query.exec(queryStr)) {
    m_lastError = query.lastError().text();
    return result;
  }
  while (query.next())
    if (colnum >= 0 && colnum < query.record().count())
      result << query.value(colnum).toString();
  return result;
}

bool REQ_SQLManager::createBackup(const QString &filename,
                                  QString *backupFilename) {
  QDir directory(
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/dbBackups");
  if (!directory.exists() && !directory.mkpath("."))
    return false;
  const QFileInfo info(filename);
  const QString destination = directory.filePath(
      info.completeBaseName() +
      QDateTime::currentDateTime().toString("_yyyyMMdd_hhmmss_zzz.") +
      info.suffix());
  if (!QFile::copy(filename, destination))
    return false;
  if (backupFilename)
    *backupFilename = destination;
  return true;
}

QSqlError REQ_SQLManager::error(const QString &text) const {
  return QSqlError("GeSiER", text, QSqlError::ConnectionError);
}
