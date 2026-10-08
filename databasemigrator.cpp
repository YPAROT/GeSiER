#include "databasemigrator.h"
#include "markdownutility.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QRegularExpression>
#include <QStringList>

bool DatabaseMigrator::execute(QSqlDatabase db, const QString &sql,
                               QString *errorMessage) {
  QSqlQuery query(db);
  if (query.exec(sql))
    return true;

  if (errorMessage)
    *errorMessage = query.lastError().text() + "\n" + sql;
  return false;
}

bool DatabaseMigrator::addColumnIfMissing(QSqlDatabase db, const QString &table,
                                          const QString &column,
                                          const QString &definition,
                                          QString *errorMessage) {
  QSqlRecord record = db.record(table);
  if (record.indexOf(column) >= 0)
    return true;
  return execute(
      db,
      QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, definition),
      errorMessage);
}

bool DatabaseMigrator::migrate(QSqlDatabase db, QString *errorMessage) {
  if (!db.isOpen()) {
    if (errorMessage)
      *errorMessage = "La base de données n'est pas ouverte.";
    return false;
  }

  QSqlQuery versionQuery("PRAGMA user_version", db);
  const int version = versionQuery.next() ? versionQuery.value(0).toInt() : 0;
  versionQuery.finish();
  if (version > CurrentVersion) {
    if (errorMessage)
      *errorMessage = QString("Cette base utilise le schéma %1, plus récent "
                              "que le schéma %2 pris en charge.")
                          .arg(version)
                          .arg(CurrentVersion);
    return false;
  }

  bool rebuildInterface = false;
  QSqlQuery interfaceInfo("PRAGMA table_info(INTERFACE)", db);
  while (interfaceInfo.next())
    if (interfaceInfo.value(1).toString().compare("DOC_ID",
                                                  Qt::CaseInsensitive) == 0)
      rebuildInterface = interfaceInfo.value(3).toBool();
  interfaceInfo.finish();
  interfaceInfo = QSqlQuery();

  bool rebuildRequirement = false;
  QStringList requirementTriggers;
  QStringList requirementIndexes;
  if (db.tables().contains("REQUIREMENT")) {
    QSqlQuery schema(db);
    schema.prepare("SELECT sql FROM sqlite_master WHERE type='table' AND "
                   "name='REQUIREMENT'");
    if (schema.exec() && schema.next()) {
      const QString tableSql = schema.value(0).toString();
      rebuildRequirement = db.record("REQUIREMENT").indexOf("PT_ID") >= 0 &&
          QRegularExpression("\\bTITLE\\b[^,)]*\\bUNIQUE\\b",
                             QRegularExpression::CaseInsensitiveOption)
              .match(tableSql)
              .hasMatch();
    }

    QSqlQuery collisions(db);
    if (!collisions.exec(
            "SELECT GROUP_CONCAT(CODE,' / ') FROM REQUIREMENT WHERE CODE IS "
            "NOT NULL GROUP BY CODE COLLATE NOCASE HAVING COUNT(*)>1")) {
      if (errorMessage)
        *errorMessage = collisions.lastError().text();
      return false;
    }
    QStringList conflicts;
    while (collisions.next())
      conflicts << collisions.value(0).toString();
    if (!conflicts.isEmpty()) {
      if (errorMessage)
        *errorMessage =
            "Des codes d'exigence ne diffèrent que par la casse : " +
            conflicts.join(", ") +
            ". Corrigez ces codes avant de relancer la migration.";
      return false;
    }

    if (rebuildRequirement) {
      QSqlQuery objects(db);
      objects.prepare("SELECT type,name,sql FROM sqlite_master WHERE "
                      "tbl_name='REQUIREMENT' AND sql IS NOT NULL AND "
                      "type IN ('trigger','index') ORDER BY type,name");
      if (!objects.exec()) {
        if (errorMessage)
          *errorMessage = objects.lastError().text();
        return false;
      }
      while (objects.next()) {
        const QString type = objects.value(0).toString();
        const QString name = objects.value(1).toString();
        const QString sql = objects.value(2).toString();
        if (type == "trigger")
          requirementTriggers << sql;
        else if (name.compare("idx_code", Qt::CaseInsensitive) != 0)
          requirementIndexes << sql;
      }
    }
  }

  const bool foreignKeys = [&db] {
    QSqlQuery q("PRAGMA foreign_keys", db);
    return q.next() && q.value(0).toBool();
  }();
  const bool rebuildV18 = version < 18;
  struct ForeignKeyRestore {
    QSqlDatabase db;
    bool restore;
    ~ForeignKeyRestore() {
      if (restore) {
        QSqlQuery q(db);
        q.exec("PRAGMA foreign_keys=ON");
      }
    }
  } restore{db, (rebuildInterface || rebuildRequirement || rebuildV18) && foreignKeys};
  if ((rebuildInterface || rebuildRequirement || rebuildV18) && foreignKeys) {
    QSqlQuery q(db);
    q.exec("PRAGMA foreign_keys=OFF");
  }

  if (!db.transaction()) {
    if (errorMessage)
      *errorMessage = db.lastError().text();
    return false;
  }

  if (rebuildRequirement) {
    const QSqlRecord old = db.record("REQUIREMENT");
    auto source = [&old](const QString &column, const QString &fallback) {
      return old.indexOf(column) >= 0 ? column : fallback;
    };
    const QString copySql =
        QString("INSERT INTO REQUIREMENT_V16(ID,PT_ID,CODE,DOC_ID,DOC_"
                "CHAPTER,TITLE,DESCRIPTION,TYPE,STATUS,SOURCE,PARENT_ID,VERIF_"
                "LEVEL,VERIF_METHOD,COMMENTS,IS_TRACE_ROOT,VERIF_PROCEDURE,"
                "REDMINE_REF,VERIF_STATUS,VERIF_MEANS) SELECT ID,PT_ID,CODE,"
                "DOC_ID,DOC_CHAPTER,TITLE,DESCRIPTION,TYPE,STATUS,SOURCE,"
                "PARENT_ID,VERIF_LEVEL,VERIF_METHOD,COMMENTS,%1,%2,%3,%4,%5 "
                "FROM REQUIREMENT")
            .arg(source("IS_TRACE_ROOT", "0"),
                 source("VERIF_PROCEDURE", "NULL"),
                 source("REDMINE_REF", "NULL"),
                 source("VERIF_STATUS", "NULL"),
                 source("VERIF_MEANS", "NULL"));
    if (!execute(
            db,
            "CREATE TABLE REQUIREMENT_V16(ID INTEGER PRIMARY KEY "
            "AUTOINCREMENT,PT_ID INTEGER NOT NULL REFERENCES PT(ID),CODE TEXT "
            "NOT NULL,DOC_ID INTEGER NOT NULL REFERENCES DOCUMENT(ID),DOC_"
            "CHAPTER INTEGER REFERENCES REQ_CHAPTER(ID),TITLE TEXT NOT NULL,"
            "DESCRIPTION TEXT,TYPE INTEGER NOT NULL REFERENCES REQ_TYPE(ID),"
            "STATUS INTEGER NOT NULL REFERENCES REQ_STATUS(ID),SOURCE TEXT,"
            "PARENT_ID INTEGER REFERENCES REQUIREMENT(ID),VERIF_LEVEL TEXT,"
            "VERIF_METHOD INTEGER NOT NULL REFERENCES REQ_METHOD(ID),COMMENTS "
            "TEXT,IS_TRACE_ROOT INTEGER NOT NULL DEFAULT 0 CHECK(IS_TRACE_ROOT "
            "IN (0,1)),VERIF_PROCEDURE TEXT,REDMINE_REF TEXT,VERIF_STATUS "
            "TEXT,VERIF_MEANS TEXT)",
            errorMessage) ||
        !execute(db, copySql, errorMessage) ||
        !execute(db, "DROP TABLE REQUIREMENT", errorMessage) ||
        !execute(db, "ALTER TABLE REQUIREMENT_V16 RENAME TO REQUIREMENT",
                 errorMessage)) {
      db.rollback();
      return false;
    }
    for (const QString &sql : requirementIndexes + requirementTriggers) {
      if (!execute(db, sql, errorMessage)) {
        db.rollback();
        return false;
      }
    }
  }

  if (rebuildInterface) {
    const QSqlRecord old = db.record("INTERFACE");
    auto source = [&old](const QString &column, const QString &fallback) {
      return old.indexOf(column) >= 0 ? column : fallback;
    };
    if (!execute(db,
                 "CREATE TABLE INTERFACE_V12(ID INTEGER PRIMARY KEY "
                 "AUTOINCREMENT,ELEMENT1 INTEGER NOT NULL REFERENCES "
                 "PT(ID),ELEMENT2 INTEGER NOT NULL REFERENCES PT(ID),DOC_ID "
                 "INTEGER REFERENCES DOCUMENT(ID),DESCRIPTION TEXT,DOC_CHAPTER "
                 "INTEGER REFERENCES IF_CHAPTER(ID),CODE TEXT,STATUS TEXT NOT "
                 "NULL DEFAULT 'DRAFT',ARCHIVED INTEGER NOT NULL DEFAULT 0 "
                 "CHECK(ARCHIVED IN (0,1)),CREATED_AT TEXT,UPDATED_AT TEXT)",
                 errorMessage) ||
        !execute(
            db,
            QString(
                "INSERT INTO "
                "INTERFACE_V12(ID,ELEMENT1,ELEMENT2,DOC_ID,DESCRIPTION,DOC_"
                "CHAPTER,CODE,STATUS,ARCHIVED,CREATED_AT,UPDATED_AT) SELECT "
                "ID,ELEMENT1,ELEMENT2,DOC_ID,DESCRIPTION,DOC_CHAPTER,%1,%2,%3,%"
                "4,%5 FROM INTERFACE")
                .arg(source("CODE", "NULL"), source("STATUS", "'DRAFT'"),
                     source("ARCHIVED", "0"), source("CREATED_AT", "NULL"),
                     source("UPDATED_AT", "NULL")),
            errorMessage) ||
        !execute(db, "DROP TABLE INTERFACE", errorMessage) ||
        !execute(db, "ALTER TABLE INTERFACE_V12 RENAME TO INTERFACE",
                 errorMessage)) {
      db.rollback();
      return false;
    }
  }

  QStringList statements;
  statements
      << "CREATE TABLE IF NOT EXISTS PROJECT_META (KEY TEXT PRIMARY KEY, VALUE "
         "TEXT)"
      << "CREATE TABLE IF NOT EXISTS REQ_TYPE (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, TYPE TEXT NOT NULL UNIQUE)"
      << "CREATE TABLE IF NOT EXISTS REQUIREMENT_RELATION_TYPE (ID INTEGER "
         "PRIMARY KEY, CODE TEXT UNIQUE NOT NULL, LABEL TEXT NOT NULL)"
      << "INSERT OR IGNORE INTO REQUIREMENT_RELATION_TYPE(ID,CODE,LABEL) "
         "VALUES (1,'DECOMPOSE','Décompose'),(2,'DERIVES_FROM','Dérive "
         "de'),(3,'DEPENDS_ON','Dépend de')"
      << "CREATE TABLE IF NOT EXISTS REQUIREMENT_RELATION (ID INTEGER PRIMARY "
         "KEY AUTOINCREMENT, SOURCE_REQ_ID INTEGER NOT NULL REFERENCES "
         "REQUIREMENT(ID) ON DELETE CASCADE, TARGET_REQ_ID INTEGER NOT NULL "
         "REFERENCES REQUIREMENT(ID) ON DELETE CASCADE, TYPE_ID INTEGER NOT "
         "NULL REFERENCES REQUIREMENT_RELATION_TYPE(ID), CREATED_AT TEXT NOT "
         "NULL DEFAULT CURRENT_TIMESTAMP, COMMENT TEXT, "
         "UNIQUE(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID), "
         "CHECK(SOURCE_REQ_ID<>TARGET_REQ_ID))"
      << "CREATE TABLE IF NOT EXISTS REQIF_IDENTITY (REQ_ID INTEGER PRIMARY "
         "KEY REFERENCES REQUIREMENT(ID) ON DELETE CASCADE, EXTERNAL_ID TEXT "
         "NOT NULL UNIQUE, SOURCE_DOCUMENT TEXT, LAST_SYNC_AT TEXT NOT NULL "
         "DEFAULT CURRENT_TIMESTAMP)"
      << "CREATE TABLE IF NOT EXISTS REQIF_ATTRIBUTE (REQ_ID INTEGER NOT NULL "
         "REFERENCES REQUIREMENT(ID) ON DELETE CASCADE, ATTRIBUTE_ID TEXT NOT "
         "NULL, ATTRIBUTE_NAME TEXT NOT NULL, VALUE TEXT, VALUE_TYPE TEXT, "
         "PRIMARY KEY(REQ_ID,ATTRIBUTE_ID))"
      << "CREATE TABLE IF NOT EXISTS REQ_IMAGE (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT,REQ_ID INTEGER NOT NULL REFERENCES REQUIREMENT(ID) ON "
         "DELETE CASCADE,TOKEN TEXT NOT NULL UNIQUE,MIME_TYPE TEXT NOT NULL,"
         "LEGEND TEXT,IMAGE BLOB NOT NULL)"
      << "CREATE TABLE IF NOT EXISTS REQUIREMENT_PT (REQ_ID INTEGER NOT NULL "
         "REFERENCES REQUIREMENT(ID) ON DELETE CASCADE, PT_ID INTEGER NOT NULL "
         "REFERENCES PT(ID) ON DELETE CASCADE, PRIMARY KEY(REQ_ID,PT_ID))"
      << "CREATE TABLE IF NOT EXISTS REQUIREMENT_VERIFICATION (ID INTEGER "
         "PRIMARY KEY AUTOINCREMENT, REQ_ID INTEGER NOT NULL REFERENCES "
         "REQUIREMENT(ID) ON DELETE CASCADE, METHOD_ID INTEGER NOT NULL "
         "REFERENCES REQ_METHOD(ID), VERIF_LEVEL TEXT, PROCEDURE_REF TEXT, "
         "REDMINE_REF TEXT, MEANS TEXT, VERDICT TEXT CHECK(VERDICT IS NULL OR "
         "VERDICT IN ('C','PC','NC')), COMMENT TEXT, POSITION INTEGER NOT NULL "
         "DEFAULT 0, UNIQUE(REQ_ID,METHOD_ID))"
      << "CREATE TABLE IF NOT EXISTS CONFIGURATION (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, CODE TEXT UNIQUE NOT NULL, LABEL TEXT NOT NULL, "
         "DESCRIPTION TEXT, ACTIVE INTEGER NOT NULL DEFAULT 1 CHECK(ACTIVE IN "
         "(0,1)))"
      << "CREATE TABLE IF NOT EXISTS REQUIREMENT_APPLICABILITY (REQ_ID INTEGER "
         "NOT NULL REFERENCES REQUIREMENT(ID) ON DELETE CASCADE, CONFIG_ID "
         "INTEGER NOT NULL REFERENCES CONFIGURATION(ID) ON DELETE CASCADE, "
         "PT_ID INTEGER REFERENCES PT(ID) ON DELETE CASCADE, APPLICABLE "
         "INTEGER NOT NULL DEFAULT 1 CHECK(APPLICABLE IN (0,1)), COMMENT TEXT, "
         "PRIMARY KEY(REQ_ID,CONFIG_ID,PT_ID))"
      << "CREATE TABLE IF NOT EXISTS DOCUMENT_REFERENCE (ID INTEGER PRIMARY "
         "KEY AUTOINCREMENT, DOC_ID INTEGER NOT NULL REFERENCES DOCUMENT(ID) "
         "ON DELETE CASCADE, REFERENCE TEXT NOT NULL, IS_PRIMARY INTEGER NOT "
         "NULL DEFAULT 0 CHECK(IS_PRIMARY IN (0,1)), UNIQUE(DOC_ID,REFERENCE))"
      << "CREATE UNIQUE INDEX IF NOT EXISTS IDX_DOCUMENT_PRIMARY_REFERENCE ON "
         "DOCUMENT_REFERENCE(DOC_ID) WHERE IS_PRIMARY=1"
      << "CREATE TABLE IF NOT EXISTS DOCUMENT_NODE (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, DOC_ID INTEGER NOT NULL REFERENCES DOCUMENT(ID) ON "
         "DELETE CASCADE, PARENT_ID INTEGER REFERENCES DOCUMENT_NODE(ID) ON "
         "DELETE CASCADE, NODE_TYPE TEXT NOT NULL CHECK(NODE_TYPE IN "
         "('CHAPTER','REQUIREMENT','TEXT','IMAGE')), POSITION INTEGER NOT NULL "
         "DEFAULT 0, TITLE TEXT, REQ_ID INTEGER REFERENCES REQUIREMENT(ID), "
         "TEXT_CONTENT TEXT, IMAGE_DATA BLOB, IMAGE_LEGEND TEXT, "
         "UNIQUE(DOC_ID,PARENT_ID,POSITION))"
      << "CREATE TABLE IF NOT EXISTS DOCUMENT_EXPORT (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, DOC_ID INTEGER NOT NULL REFERENCES DOCUMENT(ID), "
         "EXPORT_KIND TEXT NOT NULL CHECK(EXPORT_KIND IN "
         "('DRAFT','PUBLICATION')), VERSION TEXT, TITLE TEXT, AUTHOR TEXT, "
         "EXPORTED_AT TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, FILE_PATH TEXT, "
         "FILE_SHA256 TEXT, GED_REFERENCE TEXT, GED_LINK TEXT, SNAPSHOT_JSON "
         "TEXT)"
      << "CREATE TABLE IF NOT EXISTS INTERFACE_TYPE (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, CODE TEXT UNIQUE NOT NULL, LABEL TEXT NOT NULL)"
      << "INSERT OR IGNORE INTO INTERFACE_TYPE(CODE,LABEL) VALUES "
         "('MECHANICAL','Mécanique'),('ELECTRICAL','Électrique'),('"
         "COMMUNICATION','Communication / "
         "données'),('THERMAL','Thermique'),('FLUID','Fluide'),('OTHER','Autre'"
         ")"
      << "CREATE TABLE IF NOT EXISTS INTERFACE_TYPE_LINK (INTERFACE_ID INTEGER "
         "NOT NULL REFERENCES INTERFACE(ID) ON DELETE CASCADE, TYPE_ID INTEGER "
         "NOT NULL REFERENCES INTERFACE_TYPE(ID), PRIMARY "
         "KEY(INTERFACE_ID,TYPE_ID))"
      << "CREATE TABLE IF NOT EXISTS INTERFACE_DOCUMENT (INTERFACE_ID INTEGER "
         "NOT NULL REFERENCES INTERFACE(ID) ON DELETE CASCADE, DOC_ID INTEGER "
         "NOT NULL REFERENCES DOCUMENT(ID) ON DELETE CASCADE, CHAPTER_NODE_ID "
         "INTEGER REFERENCES DOCUMENT_NODE(ID), PRIMARY "
         "KEY(INTERFACE_ID,DOC_ID))"
      << "CREATE TABLE IF NOT EXISTS INTERFACE_REQUIREMENT (INTERFACE_ID "
         "INTEGER "
         "NOT NULL REFERENCES INTERFACE(ID) ON DELETE CASCADE, REQ_ID INTEGER "
         "NOT NULL REFERENCES REQUIREMENT(ID) ON DELETE CASCADE, PRIMARY "
         "KEY(INTERFACE_ID,REQ_ID))"
      << "CREATE TABLE IF NOT EXISTS CHANGE_ITEM (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, CODE TEXT UNIQUE NOT NULL, TYPE TEXT NOT NULL "
         "CHECK(TYPE IN ('CHANGE_REQUEST','WAIVER','DEVIATION','OTHER')), "
         "STATUS TEXT NOT NULL DEFAULT 'OPEN', DESCRIPTION TEXT, DECISION "
         "TEXT, OPENED_AT TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, CLOSED_AT "
         "TEXT, EXTERNAL_REFERENCE TEXT, EXTERNAL_LINK TEXT)"
      << "CREATE TABLE IF NOT EXISTS CHANGE_LINK (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, CHANGE_ID INTEGER NOT NULL REFERENCES CHANGE_ITEM(ID) "
         "ON DELETE CASCADE, OBJECT_TYPE TEXT NOT NULL CHECK(OBJECT_TYPE IN "
         "('REQUIREMENT','PT','CONFIGURATION','INTERFACE','DOCUMENT')), "
         "OBJECT_ID INTEGER NOT NULL, UNIQUE(CHANGE_ID,OBJECT_TYPE,OBJECT_ID))"
      << "CREATE TABLE IF NOT EXISTS EVENT_LOG (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, EVENT_TIME TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
         "AUTHOR TEXT, EVENT_TYPE TEXT NOT NULL, OBJECT_TYPE TEXT NOT NULL, "
         "OBJECT_ID INTEGER, BEFORE_JSON TEXT, AFTER_JSON TEXT, COMMENT TEXT)"
      << "CREATE TABLE IF NOT EXISTS CHANGE_TYPE (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, CODE TEXT UNIQUE NOT NULL, LABEL TEXT NOT NULL, "
         "ACTIVE INTEGER NOT NULL DEFAULT 1 CHECK(ACTIVE IN (0,1)))"
      << "INSERT OR IGNORE INTO CHANGE_TYPE(CODE,LABEL) VALUES "
         "('CHANGE_REQUEST','Demande de changement'),"
         "('WAIVER','Waiver / Dérogation'),('DEVIATION','Déviation'),"
         "('OTHER','Autre')"
      << "UPDATE CHANGE_TYPE SET LABEL='Demande de changement' WHERE "
         "CODE='CHANGE_REQUEST' AND LABEL='Change Request'"
      << "UPDATE CHANGE_TYPE SET LABEL='Waiver / Dérogation' WHERE "
         "CODE='WAIVER' AND LABEL='Waiver'"
      << "UPDATE CHANGE_TYPE SET LABEL='Déviation' WHERE CODE='DEVIATION' "
         "AND LABEL='Dérogation'"
      << "CREATE TABLE IF NOT EXISTS CHANGE_STATUS (ID INTEGER PRIMARY KEY "
         "AUTOINCREMENT, CODE TEXT UNIQUE NOT NULL, LABEL TEXT NOT NULL, "
         "IS_FINAL INTEGER NOT NULL DEFAULT 0 CHECK(IS_FINAL IN (0,1)), "
         "POSITION INTEGER NOT NULL DEFAULT 0)"
      << "INSERT OR IGNORE INTO CHANGE_STATUS(CODE,LABEL,IS_FINAL,POSITION) "
         "VALUES ('OPEN','Ouvert',0,10),('IN_REVIEW','En instruction',0,20),"
         "('APPROVED','Approuvé',1,30),('REJECTED','Rejeté',1,40),"
         "('CLOSED','Clos',1,50)"
      << "CREATE INDEX IF NOT EXISTS IDX_REQ_REL_SOURCE ON "
         "REQUIREMENT_RELATION(SOURCE_REQ_ID)"
      << "CREATE INDEX IF NOT EXISTS IDX_REQ_REL_TARGET ON "
         "REQUIREMENT_RELATION(TARGET_REQ_ID)"
      << "CREATE INDEX IF NOT EXISTS IDX_REQ_VERIFICATION_REQ ON "
         "REQUIREMENT_VERIFICATION(REQ_ID,POSITION)"
      << "CREATE INDEX IF NOT EXISTS IDX_DOC_NODE_DOC ON "
         "DOCUMENT_NODE(DOC_ID,PARENT_ID,POSITION)"
      << "CREATE INDEX IF NOT EXISTS IDX_EVENT_OBJECT ON "
         "EVENT_LOG(OBJECT_TYPE,OBJECT_ID,EVENT_TIME)"
      << "CREATE INDEX IF NOT EXISTS IDX_INTERFACE_ENDPOINTS ON "
         "INTERFACE(ELEMENT1,ELEMENT2)"
      << "CREATE INDEX IF NOT EXISTS IDX_INTERFACE_REQUIREMENT_REQ ON "
         "INTERFACE_REQUIREMENT(REQ_ID)";

  statements
      << "CREATE TRIGGER IF NOT EXISTS REQUIREMENT_DELETE_GUARD BEFORE DELETE "
         "ON REQUIREMENT BEGIN SELECT RAISE(ABORT,'Une exigence tracée doit "
         "être rendue obsolète, pas supprimée.'); END"
      << "CREATE TRIGGER IF NOT EXISTS REQUIREMENT_RELATION_CYCLE BEFORE "
         "INSERT ON REQUIREMENT_RELATION WHEN NEW.TYPE_ID=1 AND EXISTS(WITH "
         "RECURSIVE D(ID) AS (SELECT TARGET_REQ_ID FROM REQUIREMENT_RELATION "
         "WHERE SOURCE_REQ_ID=NEW.TARGET_REQ_ID AND TYPE_ID=1 UNION SELECT "
         "X.TARGET_REQ_ID FROM REQUIREMENT_RELATION X JOIN D ON "
         "X.SOURCE_REQ_ID=D.ID WHERE X.TYPE_ID=1) SELECT 1 FROM D WHERE "
         "ID=NEW.SOURCE_REQ_ID) BEGIN SELECT RAISE(ABORT,'Cycle de "
         "décomposition interdit.'); END"
      << "CREATE TRIGGER IF NOT EXISTS REQUIREMENT_RELATION_NEW AFTER INSERT "
         "ON REQUIREMENT_RELATION BEGIN INSERT INTO "
         "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
         "VALUES('CREATE','REQUIREMENT_RELATION',NEW.ID,json_object('source',"
         "NEW.SOURCE_REQ_ID,'target',NEW.TARGET_REQ_ID,'type',NEW.TYPE_ID)); "
         "END"
      << "CREATE TRIGGER IF NOT EXISTS REQUIREMENT_RELATION_DELETE AFTER "
         "DELETE ON REQUIREMENT_RELATION BEGIN INSERT INTO "
         "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON) "
         "VALUES('DELETE','REQUIREMENT_RELATION',OLD.ID,json_object('source',"
         "OLD.SOURCE_REQ_ID,'target',OLD.TARGET_REQ_ID,'type',OLD.TYPE_ID)); "
         "END"
      << "CREATE TRIGGER IF NOT EXISTS DOCUMENT_EXPORT_LOG AFTER INSERT ON "
         "DOCUMENT_EXPORT BEGIN INSERT INTO "
         "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) VALUES(CASE "
         "NEW.EXPORT_KIND WHEN 'DRAFT' THEN 'EXPORT_DRAFT' ELSE 'PUBLISH' "
         "END,'DOCUMENT',NEW.DOC_ID,json_object('export_id',NEW.ID,'version',"
         "NEW.VERSION,'path',NEW.FILE_PATH,'sha256',NEW.FILE_SHA256)); END"
      << "CREATE TRIGGER IF NOT EXISTS DOCUMENT_NODE_PARENT_INSERT BEFORE "
         "INSERT ON DOCUMENT_NODE WHEN NEW.PARENT_ID IS NOT NULL AND NOT "
         "EXISTS(SELECT 1 FROM DOCUMENT_NODE P WHERE P.ID=NEW.PARENT_ID AND "
         "P.DOC_ID=NEW.DOC_ID AND P.NODE_TYPE='CHAPTER') BEGIN SELECT "
         "RAISE(ABORT,'Parent de document invalide'); END"
      << "CREATE TRIGGER IF NOT EXISTS DOCUMENT_NODE_PARENT_UPDATE BEFORE "
         "UPDATE OF PARENT_ID,DOC_ID ON DOCUMENT_NODE WHEN NEW.PARENT_ID IS "
         "NOT NULL AND NOT EXISTS(SELECT 1 FROM DOCUMENT_NODE P WHERE "
         "P.ID=NEW.PARENT_ID AND P.DOC_ID=NEW.DOC_ID AND "
         "P.NODE_TYPE='CHAPTER') BEGIN SELECT RAISE(ABORT,'Parent de document "
         "invalide'); END"
      << "CREATE TRIGGER IF NOT EXISTS DOCUMENT_NODE_CYCLE_UPDATE BEFORE "
         "UPDATE OF PARENT_ID ON DOCUMENT_NODE WHEN NEW.PARENT_ID IS NOT NULL "
         "AND EXISTS(WITH RECURSIVE D(ID) AS (SELECT ID FROM DOCUMENT_NODE "
         "WHERE PARENT_ID=NEW.ID UNION ALL SELECT N.ID FROM DOCUMENT_NODE N "
         "JOIN D ON N.PARENT_ID=D.ID) SELECT 1 FROM D WHERE ID=NEW.PARENT_ID) "
         "BEGIN SELECT RAISE(ABORT,'Cycle de document interdit'); END";

  for (const auto &guard : QList<QPair<QString, QString>>{
           {"PT", "PT"},
           {"CONFIGURATION", "CONFIGURATION"},
           {"INTERFACE", "INTERFACE"},
           {"DOCUMENT", "DOCUMENT"}}) {
    statements << QString(
                      "CREATE TRIGGER IF NOT EXISTS CHANGE_%1_DELETE_GUARD "
                      "BEFORE DELETE ON %1 WHEN EXISTS(SELECT 1 FROM "
                      "CHANGE_LINK WHERE OBJECT_TYPE='%2' AND OBJECT_ID=OLD.ID) "
                      "BEGIN SELECT RAISE(ABORT,'Objet lié à un changement : "
                      "archivez-le ou retirez d''abord l''association.'); END")
                      .arg(guard.first, guard.second);
  }

  for (const QString &statement : statements) {
    if (!execute(db, statement, errorMessage)) {
      db.rollback();
      return false;
    }
  }

  const struct Column {
    const char *table;
    const char *name;
    const char *definition;
  } columns[] = {
      {"REQUIREMENT", "IS_TRACE_ROOT",
       "INTEGER NOT NULL DEFAULT 0 CHECK(IS_TRACE_ROOT IN (0,1))"},
      {"REQUIREMENT", "VERIF_PROCEDURE", "TEXT"},
      {"REQUIREMENT", "REDMINE_REF", "TEXT"},
      {"REQUIREMENT", "VERIF_STATUS", "TEXT"},
      {"REQUIREMENT", "VERIF_MEANS", "TEXT"},
      {"REQ_TYPE", "CODE", "TEXT"},
      {"DOCUMENT", "REFERENCE", "TEXT"},
      {"DOCUMENT", "TEMPLATE_PATH", "TEXT"},
      {"DOCUMENT_EXPORT", "TEMPLATE_PATH", "TEXT"},
      {"DOCUMENT_EXPORT", "TEMPLATE_SHA256", "TEXT"},
      {"DOCUMENT_EXPORT", "METADATA_JSON", "TEXT"},
      {"DOCUMENT_EXPORT", "DRAFT_LABEL", "TEXT"},
      {"DOCUMENT", "METADATA_JSON", "TEXT"},
      {"INTERFACE", "CODE", "TEXT"},
      {"INTERFACE", "STATUS", "TEXT NOT NULL DEFAULT 'DRAFT'"},
      {"INTERFACE", "ARCHIVED",
       "INTEGER NOT NULL DEFAULT 0 CHECK(ARCHIVED IN (0,1))"},
      {"INTERFACE", "CREATED_AT", "TEXT"},
      {"INTERFACE", "UPDATED_AT", "TEXT"},
      {"PT", "SEGMENT", "TEXT"},
      {"PT", "DESCRIPTION", "TEXT"},
      {"PT", "POSITION", "INTEGER NOT NULL DEFAULT 0"},
      {"PT", "ARCHIVED", "INTEGER NOT NULL DEFAULT 0 CHECK(ARCHIVED IN (0,1))"},
      {"PT", "CREATED_AT", "TEXT"},
      {"PT", "UPDATED_AT", "TEXT"},
      {"REQUIREMENT_PT", "IS_PRIMARY",
       "INTEGER NOT NULL DEFAULT 0 CHECK(IS_PRIMARY IN (0,1))"},
      {"REQUIREMENT_VERIFICATION", "VERIFICATION_LEVEL_PT_ID",
       "INTEGER REFERENCES PT(ID)"},
      {"CONFIGURATION", "POSITION", "INTEGER NOT NULL DEFAULT 0"},
      {"CONFIGURATION", "CREATED_AT", "TEXT"},
      {"CONFIGURATION", "UPDATED_AT", "TEXT"},
      {"CHANGE_ITEM", "TYPE_ID", "INTEGER REFERENCES CHANGE_TYPE(ID)"},
      {"CHANGE_ITEM", "STATUS_ID", "INTEGER REFERENCES CHANGE_STATUS(ID)"},
      {"CHANGE_ITEM", "ARCHIVED",
       "INTEGER NOT NULL DEFAULT 0 CHECK(ARCHIVED IN (0,1))"},
      {"CHANGE_ITEM", "UPDATED_AT", "TEXT"}};
  for (const Column &column : columns) {
    const QString table = QString::fromLatin1(column.table);
    const QString name = QString::fromLatin1(column.name);
    if (version >= 18 &&
        ((table == "REQUIREMENT" &&
          (name == "VERIF_PROCEDURE" || name == "REDMINE_REF" ||
           name == "VERIF_STATUS" || name == "VERIF_MEANS")) ||
         (table == "DOCUMENT" && name == "REFERENCE")))
      continue;
    if (!addColumnIfMissing(db, column.table, column.name, column.definition,
                            errorMessage)) {
      db.rollback();
      return false;
    }
  }

  if (version < 17) {
    if (db.tables().contains("REQ_IMAGE") &&
        db.record("REQ_IMAGE").indexOf("TOKEN") < 0) {
      if (!execute(db, "ALTER TABLE REQ_IMAGE RENAME TO REQ_IMAGE_V16",
                   errorMessage) ||
          !execute(db,
                   "CREATE TABLE REQ_IMAGE (ID INTEGER PRIMARY KEY "
                   "AUTOINCREMENT,REQ_ID INTEGER NOT NULL REFERENCES "
                   "REQUIREMENT(ID) ON DELETE CASCADE,TOKEN TEXT NOT NULL "
                   "UNIQUE,MIME_TYPE TEXT NOT NULL,LEGEND TEXT,IMAGE BLOB "
                   "NOT NULL)", errorMessage) ||
          !execute(db,
                   "INSERT INTO REQ_IMAGE(ID,REQ_ID,TOKEN,MIME_TYPE,LEGEND,IMAGE) "
                   "SELECT ID,REQ_ID,'legacy-'||ID,'image/png',LEGEND,IMAGE "
                   "FROM REQ_IMAGE_V16", errorMessage) ||
          !execute(db, "DROP TABLE REQ_IMAGE_V16", errorMessage)) {
        db.rollback(); return false;
      }
    }
    QSqlQuery descriptions(db);
    if (!descriptions.exec("SELECT ID,COALESCE(DESCRIPTION,'') FROM REQUIREMENT")) {
      if (errorMessage) *errorMessage = descriptions.lastError().text();
      db.rollback(); return false;
    }
    QList<QPair<int, QString>> converted;
    while (descriptions.next()) {
      const int requirementId = descriptions.value(0).toInt();
      QString markdown = MarkdownUtility::normalizeLegacy(
          descriptions.value(1).toString());
      QSqlQuery images(db);
      images.prepare("SELECT TOKEN,COALESCE(LEGEND,'') FROM REQ_IMAGE WHERE REQ_ID=? ORDER BY ID");
      images.addBindValue(requirementId);
      if (!images.exec()) {
        if (errorMessage) *errorMessage = images.lastError().text();
        db.rollback(); return false;
      }
      while (images.next()) {
        const QString token = images.value(0).toString();
        const QString url = MarkdownUtility::imageUrl(token);
        if (!markdown.contains(url))
          markdown += QString("\n\n![%1](%2)")
                          .arg(images.value(1).toString(), url);
      }
      converted << qMakePair(requirementId, markdown.trimmed());
    }
    descriptions = QSqlQuery();
    for (const auto &item : converted) {
      QSqlQuery update(db);
      update.prepare("UPDATE REQUIREMENT SET DESCRIPTION=? WHERE ID=?");
      update.addBindValue(item.second); update.addBindValue(item.first);
      if (!update.exec()) {
        if (errorMessage) *errorMessage = update.lastError().text();
        db.rollback(); return false;
      }
    }
  }

  // Le schéma historique stockait type et statut comme textes contraints.
  const QSqlRecord changeColumns = db.record("CHANGE_ITEM");
  const bool hasLegacyChangeText =
      changeColumns.indexOf("TYPE") >= 0 && changeColumns.indexOf("STATUS") >= 0;
  if (version < 18 && hasLegacyChangeText && (!execute(db,
               "UPDATE CHANGE_ITEM SET TYPE_ID=(SELECT ID FROM CHANGE_TYPE "
               "WHERE CODE=CHANGE_ITEM.TYPE) WHERE TYPE_ID IS NULL",
               errorMessage) ||
      !execute(db,
               "UPDATE CHANGE_ITEM SET STATUS_ID=(SELECT ID FROM "
               "CHANGE_STATUS WHERE CODE=UPPER(REPLACE(CHANGE_ITEM.STATUS,' "
               "','_'))) WHERE STATUS_ID IS NULL",
               errorMessage) ||
      !execute(db,
               "UPDATE CHANGE_ITEM SET TYPE_ID=(SELECT ID FROM CHANGE_TYPE "
               "WHERE CODE='OTHER') WHERE TYPE_ID IS NULL",
               errorMessage) ||
      !execute(db,
               "UPDATE CHANGE_ITEM SET STATUS_ID=(SELECT ID FROM "
               "CHANGE_STATUS WHERE CODE='OPEN') WHERE STATUS_ID IS NULL",
               errorMessage))) {
    db.rollback();
    return false;
  }
  if (version < 18 && db.record("REQUIREMENT").indexOf("PT_ID") >= 0 &&
      (!execute(db,
                "INSERT OR IGNORE INTO REQUIREMENT_PT(REQ_ID,PT_ID) SELECT "
                "ID,PT_ID FROM REQUIREMENT WHERE PT_ID IS NOT NULL",
                errorMessage) ||
       !execute(db,
                "UPDATE REQUIREMENT_PT SET IS_PRIMARY=1 WHERE PT_ID=(SELECT "
                "R.PT_ID FROM REQUIREMENT R WHERE R.ID=REQUIREMENT_PT.REQ_ID) "
                "AND NOT EXISTS(SELECT 1 FROM REQUIREMENT_PT P WHERE "
                "P.REQ_ID=REQUIREMENT_PT.REQ_ID AND P.IS_PRIMARY=1)",
                errorMessage))) {
    db.rollback();
    return false;
  }

  if (!execute(db,
               "UPDATE CHANGE_ITEM SET "
               "UPDATED_AT=COALESCE(UPDATED_AT,OPENED_AT,CURRENT_TIMESTAMP)",
               errorMessage) ||
      !execute(db,
               "CREATE INDEX IF NOT EXISTS IDX_CHANGE_STATUS ON "
               "CHANGE_ITEM(ARCHIVED,STATUS_ID,TYPE_ID)",
               errorMessage) ||
      !execute(db,
               "CREATE INDEX IF NOT EXISTS IDX_CHANGE_LINK_OBJECT ON "
               "CHANGE_LINK(OBJECT_TYPE,OBJECT_ID)",
               errorMessage)) {
    db.rollback();
    return false;
  }

  // Conserve les liens ICD des anciens fichiers dont le document était porté
  // directement par INTERFACE.
  if (version < 18 && db.record("INTERFACE").indexOf("DOC_ID") >= 0 && !execute(db,
               "INSERT OR IGNORE INTO "
               "INTERFACE_DOCUMENT(INTERFACE_ID,DOC_ID,CHAPTER_NODE_ID) SELECT "
               "ID,DOC_ID,NULL FROM INTERFACE WHERE DOC_ID IS NOT NULL",
               errorMessage)) {
    db.rollback();
    return false;
  }

  if (version < 16 &&
      (!execute(db, "DROP INDEX IF EXISTS idx_code", errorMessage) ||
       !execute(db,
                "CREATE UNIQUE INDEX idx_code ON "
                "REQUIREMENT(CODE COLLATE NOCASE)",
                errorMessage))) {
    db.rollback();
    return false;
  }

  if (!execute(db,
               "UPDATE PT SET SEGMENT=COALESCE(NULLIF(SEGMENT,''),NAME), "
               "CREATED_AT=COALESCE(CREATED_AT,CURRENT_TIMESTAMP), "
               "UPDATED_AT=COALESCE(UPDATED_AT,CURRENT_TIMESTAMP)",
               errorMessage) ||
      !execute(db,
               "INSERT OR IGNORE INTO PROJECT_META(KEY,VALUE) "
               "VALUES('pt_separator','-')",
               errorMessage) ||
      !execute(db,
               "CREATE UNIQUE INDEX IF NOT EXISTS IDX_PT_SIBLING_SEGMENT ON "
               "PT(IFNULL(PARENT,-1),SEGMENT)",
               errorMessage) ||
      !execute(db,
               "CREATE UNIQUE INDEX IF NOT EXISTS IDX_REQ_PT_PRIMARY ON "
               "REQUIREMENT_PT(REQ_ID) WHERE IS_PRIMARY=1",
               errorMessage) ||
      !execute(db,
               "CREATE UNIQUE INDEX IF NOT EXISTS IDX_DOCUMENT_REQUIREMENT_"
               "OCCURRENCE ON DOCUMENT_NODE(DOC_ID,REQ_ID) WHERE "
               "NODE_TYPE='REQUIREMENT'",
               errorMessage) ||
      !execute(db,
               "CREATE UNIQUE INDEX IF NOT EXISTS IDX_REQUIREMENT_CONFIG_"
               "SIMPLE ON REQUIREMENT_APPLICABILITY(REQ_ID,CONFIG_ID) WHERE "
               "PT_ID IS NULL",
               errorMessage) ||
      !execute(
          db,
          "CREATE TABLE IF NOT EXISTS REQUIREMENT_CODE_SEQUENCE(PT_ID INTEGER "
          "PRIMARY KEY REFERENCES PT(ID) ON DELETE CASCADE,NEXT_VALUE INTEGER "
          "NOT NULL DEFAULT 1 CHECK(NEXT_VALUE>0))",
          errorMessage) ||
      !execute(db,
               "UPDATE CONFIGURATION SET "
               "CREATED_AT=COALESCE(CREATED_AT,CURRENT_TIMESTAMP), "
               "UPDATED_AT=COALESCE(UPDATED_AT,CURRENT_TIMESTAMP)",
               errorMessage) ||
      !execute(db,
               "CREATE INDEX IF NOT EXISTS IDX_CONFIGURATION_ORDER ON "
               "CONFIGURATION(ACTIVE DESC,POSITION,CODE)",
               errorMessage) ||
      !execute(db,
               "CREATE INDEX IF NOT EXISTS IDX_APPLICABILITY_CONFIG ON "
               "REQUIREMENT_APPLICABILITY(CONFIG_ID,PT_ID,REQ_ID)",
               errorMessage)) {
    db.rollback();
    return false;
  }

  if (version < 5 &&
      !execute(db,
               "INSERT OR IGNORE INTO "
               "REQUIREMENT_VERIFICATION(REQ_ID,METHOD_ID,VERIF_LEVEL,"
               "PROCEDURE_REF,REDMINE_REF,MEANS,VERDICT,POSITION) SELECT "
               "ID,VERIF_METHOD,VERIF_LEVEL,VERIF_PROCEDURE,REDMINE_REF,VERIF_"
               "MEANS,CASE WHEN UPPER(TRIM(COALESCE(VERIF_STATUS,''))) IN "
               "('C','PC','NC') THEN UPPER(TRIM(VERIF_STATUS)) END,0 FROM "
               "REQUIREMENT WHERE VERIF_METHOD IS NOT NULL",
               errorMessage)) {
    db.rollback();
    return false;
  }

  if (version < 6 &&
      !execute(db,
               "UPDATE REQUIREMENT_VERIFICATION SET VERIFICATION_LEVEL_PT_ID="
               "(SELECT P.ID FROM PT P WHERE UPPER(TRIM(P.SEGMENT))="
               "UPPER(TRIM(REQUIREMENT_VERIFICATION.VERIF_LEVEL)) AND "
               "(SELECT COUNT(*) FROM PT P2 WHERE UPPER(TRIM(P2.SEGMENT))="
               "UPPER(TRIM(REQUIREMENT_VERIFICATION.VERIF_LEVEL)))=1) WHERE "
               "VERIFICATION_LEVEL_PT_ID IS NULL AND "
               "LENGTH(TRIM(COALESCE(VERIF_LEVEL,'')))>0",
               errorMessage)) {
    db.rollback();
    return false;
  }

  if (version < 7) {
    const bool hasLegacyChapters = db.tables().contains("REQ_CHAPTER");
    if ((hasLegacyChapters &&
         !execute(db,
                  "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,"
                  "NODE_TYPE,POSITION,TITLE) SELECT C.DOC_ID,NULL,'CHAPTER',"
                  "1000000+C.ID,C.CHAPTER FROM REQ_CHAPTER C",
                  errorMessage)) ||
        !execute(
            db,
            hasLegacyChapters
                ? "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,"
                  "NODE_TYPE,POSITION,REQ_ID) SELECT R.DOC_ID,(SELECT N.ID "
                  "FROM DOCUMENT_NODE N JOIN REQ_CHAPTER C ON "
                  "C.DOC_ID=N.DOC_ID "
                  "AND C.CHAPTER=N.TITLE WHERE C.ID=R.DOC_CHAPTER AND "
                  "N.NODE_TYPE='CHAPTER' LIMIT "
                  "1),'REQUIREMENT',2000000+R.ID,R.ID FROM "
                  "REQUIREMENT R WHERE R.DOC_ID IS NOT NULL"
                : "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,"
                  "NODE_TYPE,POSITION,REQ_ID) SELECT "
                  "R.DOC_ID,NULL,'REQUIREMENT',"
                  "2000000+R.ID,R.ID FROM REQUIREMENT R WHERE R.DOC_ID IS NOT "
                  "NULL",
            errorMessage)) {
      db.rollback();
      return false;
    }
  }

  if (!execute(
          db,
          "CREATE TRIGGER IF NOT EXISTS DOCUMENT_PUBLICATION_IMMUTABLE "
          "BEFORE UPDATE ON DOCUMENT_EXPORT WHEN OLD.EXPORT_KIND='PUBLICATION' "
          "AND (NEW.DOC_ID<>OLD.DOC_ID OR NEW.EXPORT_KIND<>OLD.EXPORT_KIND OR "
          "IFNULL(NEW.VERSION,'')<>IFNULL(OLD.VERSION,'') OR "
          "IFNULL(NEW.TITLE,'')<>IFNULL(OLD.TITLE,'') OR "
          "IFNULL(NEW.AUTHOR,'')<>IFNULL(OLD.AUTHOR,'') OR "
          "NEW.EXPORTED_AT<>OLD.EXPORTED_AT OR "
          "IFNULL(NEW.FILE_PATH,'')<>IFNULL(OLD.FILE_PATH,'') OR "
          "IFNULL(NEW.FILE_SHA256,'')<>IFNULL(OLD.FILE_SHA256,'') OR "
          "IFNULL(NEW.SNAPSHOT_JSON,'')<>IFNULL(OLD.SNAPSHOT_JSON,'') OR "
          "IFNULL(NEW.TEMPLATE_PATH,'')<>IFNULL(OLD.TEMPLATE_PATH,'') OR "
          "IFNULL(NEW.TEMPLATE_SHA256,'')<>IFNULL(OLD.TEMPLATE_SHA256,'') OR "
          "IFNULL(NEW.METADATA_JSON,'')<>IFNULL(OLD.METADATA_JSON,'')) "
          "BEGIN SELECT RAISE(ABORT,'Une publication est immuable'); END",
          errorMessage) ||
      !execute(
          db,
          "CREATE TRIGGER IF NOT EXISTS DOCUMENT_PUBLICATION_NODELETE "
          "BEFORE DELETE ON DOCUMENT_EXPORT WHEN OLD.EXPORT_KIND='PUBLICATION' "
          "BEGIN SELECT RAISE(ABORT,'Une publication est immuable'); END",
               errorMessage)) {
    db.rollback();
    return false;
  }

  // v15: contexte d'audit commun, regroupement transactionnel et journal
  // des objets de premier niveau qui n'étaient pas encore couverts.
  if (!addColumnIfMissing(db, "EVENT_LOG", "EVENT_GROUP", "TEXT",
                          errorMessage) ||
      !execute(db, "CREATE INDEX IF NOT EXISTS IDX_EVENT_GROUP ON "
                   "EVENT_LOG(EVENT_GROUP)", errorMessage) ||
      !execute(db, "CREATE TABLE IF NOT EXISTS EVENT_CONTEXT (ID INTEGER "
                   "PRIMARY KEY CHECK(ID=1),AUTHOR TEXT NOT NULL DEFAULT "
                   "'Utilisateur',EVENT_GROUP TEXT)", errorMessage) ||
      !execute(db, "INSERT OR IGNORE INTO EVENT_CONTEXT(ID,AUTHOR) "
                   "VALUES(1,'Utilisateur')", errorMessage) ||
      !execute(db, "CREATE TABLE IF NOT EXISTS MIGRATION_REPORT (ID INTEGER "
                   "PRIMARY KEY AUTOINCREMENT,RUN_AT TEXT NOT NULL DEFAULT "
                   "CURRENT_TIMESTAMP,FROM_VERSION INTEGER NOT NULL,"
                   "TO_VERSION INTEGER NOT NULL,REPORT TEXT NOT NULL)",
               errorMessage) ||
      !execute(db, QString("INSERT INTO MIGRATION_REPORT(FROM_VERSION,"
                           "TO_VERSION,REPORT) SELECT %1,%2,'Migration "
                           "transactionnelle terminée; aucune ambiguïté "
                           "bloquante.' WHERE %1<%2 AND NOT EXISTS(SELECT 1 "
                           "FROM MIGRATION_REPORT WHERE FROM_VERSION=%1 AND "
                           "TO_VERSION=%2)").arg(version).arg(CurrentVersion),
               errorMessage)) {
    db.rollback();
    return false;
  }

  const QStringList auditTriggers = {
      "CREATE TRIGGER IF NOT EXISTS AUDIT_EVENT_CONTEXT AFTER INSERT ON EVENT_LOG WHEN NEW.AUTHOR IS NULL OR NEW.AUTHOR='' BEGIN UPDATE EVENT_LOG SET AUTHOR=(SELECT AUTHOR FROM EVENT_CONTEXT WHERE ID=1),EVENT_GROUP=COALESCE(NEW.EVENT_GROUP,(SELECT EVENT_GROUP FROM EVENT_CONTEXT WHERE ID=1)) WHERE ID=NEW.ID; END",
      "CREATE TRIGGER IF NOT EXISTS AUDIT_PT_INSERT AFTER INSERT ON PT BEGIN "
      "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
      "SELECT AUTHOR,EVENT_GROUP,'CREATE','PT',NEW.ID,json_object('name',NEW.NAME,'parent',NEW.PARENT) FROM EVENT_CONTEXT WHERE ID=1; END",
      "CREATE TRIGGER IF NOT EXISTS AUDIT_PT_UPDATE AFTER UPDATE ON PT BEGIN "
      "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON) "
      "SELECT AUTHOR,EVENT_GROUP,'UPDATE','PT',NEW.ID,json_object('name',OLD.NAME,'parent',OLD.PARENT),json_object('name',NEW.NAME,'parent',NEW.PARENT) FROM EVENT_CONTEXT WHERE ID=1; END",
      "CREATE TRIGGER IF NOT EXISTS AUDIT_PT_DELETE AFTER DELETE ON PT BEGIN "
      "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON) "
      "SELECT AUTHOR,EVENT_GROUP,'DELETE','PT',OLD.ID,json_object('name',OLD.NAME,'parent',OLD.PARENT) FROM EVENT_CONTEXT WHERE ID=1; END",
      "CREATE TRIGGER IF NOT EXISTS AUDIT_DOCUMENT_INSERT AFTER INSERT ON DOCUMENT BEGIN "
      "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
      "SELECT AUTHOR,EVENT_GROUP,'CREATE','DOCUMENT',NEW.ID,json_object('title',NEW.TITLE,'description',NEW.DESCRIPTION) FROM EVENT_CONTEXT WHERE ID=1; END",
      "CREATE TRIGGER IF NOT EXISTS AUDIT_DOCUMENT_UPDATE AFTER UPDATE ON DOCUMENT BEGIN "
      "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON) "
      "SELECT AUTHOR,EVENT_GROUP,'UPDATE','DOCUMENT',NEW.ID,json_object('title',OLD.TITLE,'description',OLD.DESCRIPTION),json_object('title',NEW.TITLE,'description',NEW.DESCRIPTION) FROM EVENT_CONTEXT WHERE ID=1; END",
      "CREATE TRIGGER IF NOT EXISTS AUDIT_DOCUMENT_DELETE AFTER DELETE ON DOCUMENT BEGIN "
      "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON) "
      "SELECT AUTHOR,EVENT_GROUP,'DELETE','DOCUMENT',OLD.ID,json_object('title',OLD.TITLE,'description',OLD.DESCRIPTION) FROM EVENT_CONTEXT WHERE ID=1; END",
      "CREATE TRIGGER IF NOT EXISTS AUDIT_GED_UPDATE AFTER UPDATE OF GED_REFERENCE,GED_LINK ON DOCUMENT_EXPORT WHEN NEW.GED_REFERENCE IS NOT OLD.GED_REFERENCE OR NEW.GED_LINK IS NOT OLD.GED_LINK BEGIN "
      "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON) "
      "SELECT AUTHOR,EVENT_GROUP,'GED_ASSOCIATION','DOCUMENT',NEW.DOC_ID,json_object('reference',OLD.GED_REFERENCE,'link',OLD.GED_LINK),json_object('reference',NEW.GED_REFERENCE,'link',NEW.GED_LINK) FROM EVENT_CONTEXT WHERE ID=1; END"};
  for (const QString &trigger : auditTriggers) {
    if (!execute(db, trigger, errorMessage)) { db.rollback(); return false; }
  }

  if (!execute(
          db,
          "CREATE TRIGGER IF NOT EXISTS CONFIGURATION_NEW AFTER INSERT ON "
          "CONFIGURATION BEGIN "
          "INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
          "VALUES('CREATE','CONFIGURATION',NEW.ID,json_object('code',NEW.CODE,'"
          "label',NEW.LABEL,'active',NEW.ACTIVE,'position',NEW.POSITION)); END",
          errorMessage) ||
      !execute(
          db,
          "CREATE TRIGGER IF NOT EXISTS CONFIGURATION_UPDATE AFTER UPDATE ON "
          "CONFIGURATION BEGIN "
          "UPDATE CONFIGURATION SET UPDATED_AT=CURRENT_TIMESTAMP WHERE "
          "ID=NEW.ID AND UPDATED_AT IS OLD.UPDATED_AT; "
          "INSERT INTO "
          "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON) "
          "VALUES(CASE WHEN OLD.ACTIVE<>NEW.ACTIVE THEN CASE NEW.ACTIVE WHEN 1 "
          "THEN 'RESTORE' ELSE 'ARCHIVE' END ELSE 'UPDATE' "
          "END,'CONFIGURATION',NEW.ID,json_object('code',OLD.CODE,'label',OLD."
          "LABEL,'description',OLD.DESCRIPTION,'active',OLD.ACTIVE,'position',"
          "OLD.POSITION),json_object('code',NEW.CODE,'label',NEW.LABEL,'"
          "description',NEW.DESCRIPTION,'active',NEW.ACTIVE,'position',NEW."
          "POSITION)); END",
          errorMessage) ||
      !execute(db,
               "CREATE TRIGGER IF NOT EXISTS APPLICABILITY_NEW AFTER INSERT ON "
               "REQUIREMENT_APPLICABILITY BEGIN INSERT INTO "
               "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,AFTER_JSON) "
               "VALUES('CREATE','REQUIREMENT_APPLICABILITY',NEW.REQ_ID,json_"
               "object('configuration',NEW.CONFIG_ID,'pt',NEW.PT_ID,'"
               "applicable',NEW.APPLICABLE,'comment',NEW.COMMENT)); END",
               errorMessage) ||
      !execute(
          db,
          "CREATE TRIGGER IF NOT EXISTS APPLICABILITY_UPDATE AFTER UPDATE ON "
          "REQUIREMENT_APPLICABILITY BEGIN INSERT INTO "
          "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON) "
          "VALUES('UPDATE','REQUIREMENT_APPLICABILITY',NEW.REQ_ID,json_object('"
          "configuration',OLD.CONFIG_ID,'pt',OLD.PT_ID,'applicable',OLD."
          "APPLICABLE,'comment',OLD.COMMENT),json_object('configuration',NEW."
          "CONFIG_ID,'pt',NEW.PT_ID,'applicable',NEW.APPLICABLE,'comment',NEW."
          "COMMENT)); END",
          errorMessage) ||
      !execute(db,
               "CREATE TRIGGER IF NOT EXISTS APPLICABILITY_DELETE AFTER DELETE "
               "ON REQUIREMENT_APPLICABILITY BEGIN INSERT INTO "
               "EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON) "
               "VALUES('DELETE','REQUIREMENT_APPLICABILITY',OLD.REQ_ID,json_"
               "object('configuration',OLD.CONFIG_ID,'pt',OLD.PT_ID,'"
               "applicable',OLD.APPLICABLE,'comment',OLD.COMMENT)); END",
               errorMessage)) {
    db.rollback();
    return false;
  }

  if (version < 18) {
    const bool hasLegacyModel = db.tables().contains("MODEL") &&
                                db.tables().contains("PART") &&
                                db.record("PART").indexOf("MODEL") >= 0;
    const bool hasLegacyChapters = db.tables().contains("REQ_CHAPTER");
    const QSqlRecord requirementColumns = db.record("REQUIREMENT");
    const bool hasLegacyRequirement =
        requirementColumns.indexOf("PARENT_ID") >= 0 &&
        requirementColumns.indexOf("PT_ID") >= 0 &&
        requirementColumns.indexOf("DOC_ID") >= 0;
    const bool hasLegacyDocumentReference =
        db.record("DOCUMENT").indexOf("REFERENCE") >= 0;
    const bool hasLegacyChanges = db.tables().contains("CONF_CHANGE") &&
                                  db.tables().contains("CC_STATUS");
    const bool hasLegacyLogs = db.tables().contains("REQ_LOGS");
    const QString documentNodeBackfill = hasLegacyChapters
        ? "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,NODE_TYPE,"
          "POSITION,REQ_ID) SELECT R.DOC_ID,(SELECT N.ID FROM DOCUMENT_NODE N "
          "JOIN REQ_CHAPTER C ON C.ID=R.DOC_CHAPTER WHERE N.DOC_ID=R.DOC_ID "
          "AND N.NODE_TYPE='CHAPTER' AND N.TITLE=C.CHAPTER ORDER BY N.ID LIMIT "
          "1),'REQUIREMENT',2000000+R.ID,R.ID FROM REQUIREMENT R WHERE R.DOC_ID "
          "IS NOT NULL AND NOT EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE "
          "N.REQ_ID=R.ID AND N.NODE_TYPE='REQUIREMENT')"
        : "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,NODE_TYPE,"
          "POSITION,REQ_ID) SELECT R.DOC_ID,NULL,'REQUIREMENT',2000000+R.ID,"
          "R.ID FROM REQUIREMENT R WHERE R.DOC_ID IS NOT NULL AND NOT EXISTS("
          "SELECT 1 FROM DOCUMENT_NODE N WHERE N.REQ_ID=R.ID AND "
          "N.NODE_TYPE='REQUIREMENT')";
    const QStringList v18 = {
        // Record discrepancies before completing missing normalized data.
        "INSERT INTO MIGRATION_REPORT(FROM_VERSION,TO_VERSION,REPORT) "
        "SELECT 17,18,'Conflit PARENT_ID pour REQUIREMENT '||R.ID||"
        "' : relation DECOMPOSE normalisée conservée' FROM REQUIREMENT R "
        "WHERE R.PARENT_ID IS NOT NULL AND EXISTS(SELECT 1 FROM "
        "REQUIREMENT_RELATION L WHERE L.TARGET_REQ_ID=R.ID AND L.TYPE_ID=1 "
        "AND L.SOURCE_REQ_ID<>R.PARENT_ID)",
        "INSERT INTO MIGRATION_REPORT(FROM_VERSION,TO_VERSION,REPORT) "
        "SELECT 17,18,'Conflit PT_ID pour REQUIREMENT '||R.ID||"
        "' : allocation normalisée conservée' FROM REQUIREMENT R WHERE "
        "R.PT_ID IS NOT NULL AND EXISTS(SELECT 1 FROM REQUIREMENT_PT P WHERE "
        "P.REQ_ID=R.ID AND P.IS_PRIMARY=1 AND P.PT_ID<>R.PT_ID)",
        "INSERT INTO MIGRATION_REPORT(FROM_VERSION,TO_VERSION,REPORT) "
        "SELECT 17,18,'Conflit DOCUMENT.REFERENCE pour DOCUMENT '||D.ID||"
        "' : référence normalisée conservée' FROM DOCUMENT D WHERE "
        "LENGTH(TRIM(COALESCE(D.REFERENCE,'')))>0 AND EXISTS(SELECT 1 FROM "
        "DOCUMENT_REFERENCE X WHERE X.DOC_ID=D.ID AND X.IS_PRIMARY=1 AND "
        "X.REFERENCE<>D.REFERENCE)",
        "INSERT INTO MIGRATION_REPORT(FROM_VERSION,TO_VERSION,REPORT) SELECT "
        "17,18,'Conflit MODEL '||M.ID||' : CONFIGURATION '||M.MODEL||"
        "' existante conservée' FROM MODEL M JOIN CONFIGURATION C ON "
        "UPPER(C.CODE)=UPPER(M.MODEL) WHERE C.LABEL<>M.MODEL",

        // Complete normalized structures only when they are still empty.
        "INSERT OR IGNORE INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_"
        "ID,TYPE_ID,COMMENT) SELECT PARENT_ID,ID,1,'Relation migrée depuis "
        "PARENT_ID' FROM REQUIREMENT R WHERE PARENT_ID IS NOT NULL AND NOT "
        "EXISTS(SELECT 1 FROM REQUIREMENT_RELATION L WHERE L.TARGET_REQ_ID="
        "R.ID AND L.TYPE_ID=1)",
        "INSERT OR IGNORE INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) SELECT "
        "ID,PT_ID,1 FROM REQUIREMENT R WHERE PT_ID IS NOT NULL AND NOT "
        "EXISTS(SELECT 1 FROM REQUIREMENT_PT P WHERE P.REQ_ID=R.ID)",
        "INSERT OR IGNORE INTO REQUIREMENT_VERIFICATION(REQ_ID,METHOD_ID,"
        "VERIFICATION_LEVEL_PT_ID,VERIF_LEVEL,PROCEDURE_REF,REDMINE_REF,MEANS,"
        "VERDICT,POSITION) SELECT ID,VERIF_METHOD,NULL,VERIF_LEVEL,VERIF_"
        "PROCEDURE,REDMINE_REF,VERIF_MEANS,CASE WHEN UPPER(TRIM(COALESCE("
        "VERIF_STATUS,''))) IN ('C','PC','NC') THEN UPPER(TRIM(VERIF_STATUS)) "
        "END,0 FROM REQUIREMENT R WHERE VERIF_METHOD IS NOT NULL AND NOT "
        "EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V WHERE V.REQ_ID=R.ID)",
        "CREATE TABLE IF NOT EXISTS DOC_TYPE(ID INTEGER PRIMARY KEY "
        "AUTOINCREMENT,TYPE TEXT NOT NULL,DESCRIPTION TEXT)",
        "INSERT INTO DOC_TYPE(TYPE,DESCRIPTION) SELECT 'SP','Specification "
        "document' WHERE NOT EXISTS(SELECT 1 FROM DOC_TYPE)",
        "INSERT OR IGNORE INTO DOCUMENT(ID,PT_ID,TYPE,TITLE,DESCRIPTION) "
        "SELECT R.DOC_ID,COALESCE(R.PT_ID,(SELECT ID FROM PT ORDER BY ID "
        "LIMIT 1)),(SELECT ID FROM DOC_TYPE ORDER BY ID LIMIT 1),'Document "
        "migré #'||R.DOC_ID,'Document recréé depuis le rattachement d''une "
        "exigence historique' FROM REQUIREMENT R WHERE R.DOC_ID IS NOT NULL "
        "AND NOT EXISTS(SELECT 1 FROM DOCUMENT D WHERE D.ID=R.DOC_ID) GROUP "
        "BY R.DOC_ID",
        "INSERT OR IGNORE INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) "
        "SELECT ID,REFERENCE,1 FROM DOCUMENT D WHERE LENGTH(TRIM(COALESCE("
        "REFERENCE,'')))>0 AND NOT EXISTS(SELECT 1 FROM DOCUMENT_REFERENCE X "
        "WHERE X.DOC_ID=D.ID AND X.IS_PRIMARY=1)",
        documentNodeBackfill,
        "INSERT OR IGNORE INTO CONFIGURATION(CODE,LABEL,DESCRIPTION,ACTIVE,"
        "POSITION,CREATED_AT,UPDATED_AT) SELECT MODEL,MODEL,'Configuration "
        "migrée depuis MODEL',1,ID*10,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP "
        "FROM MODEL",

        // Convert the two historical audit/change subsystems.
        "INSERT OR IGNORE INTO CHANGE_ITEM(CODE,TYPE,STATUS,TYPE_ID,STATUS_ID,"
        "DESCRIPTION,OPENED_AT,CLOSED_AT,ARCHIVED,UPDATED_AT) SELECT "
        "'LEGACY-CC-'||C.ID,'OTHER',CASE WHEN S.STATUS IN ('APPROVED','REJECTED',"
        "'CLOSED') THEN S.STATUS ELSE 'OPEN' END,(SELECT ID FROM CHANGE_TYPE "
        "WHERE CODE='OTHER'),(SELECT ID FROM CHANGE_STATUS WHERE CODE=CASE "
        "WHEN S.STATUS IN ('APPROVED','REJECTED','CLOSED') THEN S.STATUS ELSE "
        "'OPEN' END),C.DESCRIPTION,CURRENT_TIMESTAMP,CASE WHEN S.STATUS IN "
        "('APPROVED','REJECTED','CLOSED') THEN CURRENT_TIMESTAMP END,0,"
        "CURRENT_TIMESTAMP FROM CONF_CHANGE C JOIN CC_STATUS S ON S.ID=C.STATUS",
        "INSERT OR IGNORE INTO CHANGE_LINK(CHANGE_ID,OBJECT_TYPE,OBJECT_ID) "
        "SELECT N.ID,'REQUIREMENT',C.REQ_ID FROM CONF_CHANGE C JOIN CHANGE_ITEM "
        "N ON N.CODE='LEGACY-CC-'||C.ID",
        "INSERT INTO EVENT_LOG(EVENT_TIME,EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,"
        "BEFORE_JSON,AFTER_JSON,COMMENT) SELECT LOG_TIME,USER_ACTION,"
        "'REQUIREMENT',REQ_ID,json_object('pt',OLD_PT_ID,'document',OLD_DOC_ID,"
        "'chapter',OLD_DOC_CHAPTER,'title',OLD_TITLE,'description',OLD_"
        "DESCRIPTION,'type',OLD_TYPE,'status',OLD_STATUS,'source',OLD_SOURCE,"
        "'parent',OLD_PARENT_ID,'verificationLevel',OLD_VERIF_LEVEL,"
        "'verificationMethod',OLD_VERIF_METHOD,'comments',OLD_COMMENTS),"
        "json_object('pt',NEW_PT_ID,'document',NEW_DOC_ID,'chapter',NEW_DOC_"
        "CHAPTER,'title',NEW_TITLE,'description',NEW_DESCRIPTION,'type',NEW_"
        "TYPE,'status',NEW_STATUS,'source',NEW_SOURCE,'parent',NEW_PARENT_ID,"
        "'verificationLevel',NEW_VERIF_LEVEL,'verificationMethod',NEW_VERIF_"
        "METHOD,'comments',NEW_COMMENTS),'Historique converti depuis REQ_LOGS' "
        "FROM REQ_LOGS",

        // Remove triggers bound to historical columns before rebuilding.
        "DROP TRIGGER IF EXISTS REQ_NEW", "DROP TRIGGER IF EXISTS REQ_UPDATE",
        "DROP TRIGGER IF EXISTS REQ_DELETE",
        "DROP TRIGGER IF EXISTS REQUIREMENT_DELETE_GUARD",
        "DROP TRIGGER IF EXISTS AUDIT_DOCUMENT_INSERT",
        "DROP TRIGGER IF EXISTS AUDIT_DOCUMENT_UPDATE",
        "DROP TRIGGER IF EXISTS AUDIT_DOCUMENT_DELETE",

        "INSERT INTO REQ_STATUS(SHORTCUT,STATUS) SELECT 'D','Draft' WHERE NOT "
        "EXISTS(SELECT 1 FROM REQ_STATUS)",

        "CREATE TABLE REQUIREMENT_V18(ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "CODE TEXT NOT NULL,TITLE TEXT NOT NULL,DESCRIPTION TEXT,TYPE INTEGER "
        "NOT NULL REFERENCES REQ_TYPE(ID),STATUS INTEGER NOT NULL REFERENCES "
        "REQ_STATUS(ID),SOURCE TEXT,IS_TRACE_ROOT INTEGER NOT NULL DEFAULT 0 "
        "CHECK(IS_TRACE_ROOT IN (0,1)),COMMENTS TEXT)",
        "INSERT INTO REQUIREMENT_V18 SELECT ID,CODE,TITLE,DESCRIPTION,"
        "COALESCE(TYPE,(SELECT ID FROM REQ_TYPE ORDER BY ID LIMIT 1)),"
        "COALESCE(STATUS,(SELECT ID FROM REQ_STATUS ORDER BY ID LIMIT 1)),"
        "SOURCE,IS_TRACE_ROOT,COMMENTS FROM REQUIREMENT",
        "DROP TABLE REQUIREMENT",
        "ALTER TABLE REQUIREMENT_V18 RENAME TO REQUIREMENT",
        "CREATE UNIQUE INDEX idx_code ON REQUIREMENT(CODE COLLATE NOCASE)",

        "CREATE TABLE DOCUMENT_V18(ID INTEGER PRIMARY KEY AUTOINCREMENT,PT_ID "
        "INTEGER NOT NULL REFERENCES PT(ID),TYPE INTEGER NOT NULL REFERENCES "
        "DOC_TYPE(ID),TITLE TEXT NOT NULL UNIQUE,DESCRIPTION TEXT,TEMPLATE_PATH "
        "TEXT,METADATA_JSON TEXT)",
        "INSERT INTO DOCUMENT_V18 SELECT ID,PT_ID,TYPE,TITLE,DESCRIPTION,"
        "TEMPLATE_PATH,METADATA_JSON FROM DOCUMENT",
        "DROP TABLE DOCUMENT", "ALTER TABLE DOCUMENT_V18 RENAME TO DOCUMENT",

        "CREATE TABLE INTERFACE_V18(ID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "ELEMENT1 INTEGER NOT NULL REFERENCES PT(ID),ELEMENT2 INTEGER NOT NULL "
        "REFERENCES PT(ID),DESCRIPTION TEXT,CODE TEXT,STATUS TEXT NOT NULL "
        "DEFAULT 'DRAFT',ARCHIVED INTEGER NOT NULL DEFAULT 0 CHECK(ARCHIVED IN "
        "(0,1)),CREATED_AT TEXT,UPDATED_AT TEXT)",
        "INSERT INTO INTERFACE_V18 SELECT ID,ELEMENT1,ELEMENT2,DESCRIPTION,CODE,"
        "STATUS,ARCHIVED,CREATED_AT,UPDATED_AT FROM INTERFACE",
        "DROP TABLE INTERFACE", "ALTER TABLE INTERFACE_V18 RENAME TO INTERFACE",

        "CREATE TABLE CHANGE_ITEM_V18(ID INTEGER PRIMARY KEY AUTOINCREMENT,CODE "
        "TEXT UNIQUE NOT NULL,TYPE_ID INTEGER NOT NULL REFERENCES CHANGE_TYPE("
        "ID),STATUS_ID INTEGER NOT NULL REFERENCES CHANGE_STATUS(ID),DESCRIPTION "
        "TEXT,DECISION TEXT,OPENED_AT TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "CLOSED_AT TEXT,EXTERNAL_REFERENCE TEXT,EXTERNAL_LINK TEXT,ARCHIVED "
        "INTEGER NOT NULL DEFAULT 0 CHECK(ARCHIVED IN (0,1)),UPDATED_AT TEXT)",
        "INSERT INTO CHANGE_ITEM_V18 SELECT ID,CODE,TYPE_ID,STATUS_ID,DESCRIPTION,"
        "DECISION,OPENED_AT,CLOSED_AT,EXTERNAL_REFERENCE,EXTERNAL_LINK,ARCHIVED,"
        "UPDATED_AT FROM CHANGE_ITEM",
        "DROP TABLE CHANGE_ITEM", "ALTER TABLE CHANGE_ITEM_V18 RENAME TO CHANGE_ITEM",

        "CREATE TABLE PART_V18(ID INTEGER PRIMARY KEY AUTOINCREMENT,PT_ID "
        "INTEGER NOT NULL REFERENCES PT(ID),NAME TEXT NOT NULL UNIQUE,SERIAL "
        "INTEGER NOT NULL UNIQUE,CONFIG_ID INTEGER NOT NULL REFERENCES "
        "CONFIGURATION(ID),DESCRIPTION TEXT)",
        "INSERT INTO PART_V18 SELECT P.ID,P.PT_ID,P.NAME,P.SERIAL,C.ID,P."
        "DESCRIPTION FROM PART P JOIN MODEL M ON M.ID=P.MODEL JOIN "
        "CONFIGURATION C ON UPPER(C.CODE)=UPPER(M.MODEL)",
        "DROP TABLE PART", "ALTER TABLE PART_V18 RENAME TO PART",
        "DROP TABLE MODEL",

        "DROP TABLE IF EXISTS REDMINE_ISSUES", "DROP TABLE IF EXISTS REQ_LOGS",
        "DROP TABLE IF EXISTS CONF_CHANGE", "DROP TABLE IF EXISTS CC_STATUS",
        "DROP TABLE IF EXISTS REQ_CHAPTER", "DROP TABLE IF EXISTS IF_CHAPTER",
        "CREATE UNIQUE INDEX IDX_CHANGE_STATUS ON CHANGE_ITEM(ARCHIVED,STATUS_ID,TYPE_ID)",
        "CREATE INDEX IDX_INTERFACE_ENDPOINTS ON INTERFACE(ELEMENT1,ELEMENT2)",
        "CREATE TRIGGER CHANGE_DOCUMENT_DELETE_GUARD BEFORE DELETE ON DOCUMENT "
        "WHEN EXISTS(SELECT 1 FROM CHANGE_LINK WHERE OBJECT_TYPE='DOCUMENT' "
        "AND OBJECT_ID=OLD.ID) BEGIN SELECT RAISE(ABORT,'Objet lié à un "
        "changement : archivez-le ou retirez d''abord l''association.'); END",
        "CREATE TRIGGER CHANGE_INTERFACE_DELETE_GUARD BEFORE DELETE ON "
        "INTERFACE WHEN EXISTS(SELECT 1 FROM CHANGE_LINK WHERE OBJECT_TYPE="
        "'INTERFACE' AND OBJECT_ID=OLD.ID) BEGIN SELECT RAISE(ABORT,'Objet lié "
        "à un changement : archivez-le ou retirez d''abord l''association.'); END",
        "CREATE TRIGGER REQUIREMENT_DELETE_GUARD BEFORE DELETE ON REQUIREMENT "
        "BEGIN SELECT RAISE(ABORT,'Une exigence tracée doit être rendue "
        "obsolète, pas supprimée.'); END",
        "CREATE TRIGGER AUDIT_DOCUMENT_INSERT AFTER INSERT ON DOCUMENT BEGIN "
        "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,"
        "OBJECT_ID,AFTER_JSON) SELECT AUTHOR,EVENT_GROUP,'CREATE','DOCUMENT',"
        "NEW.ID,json_object('title',NEW.TITLE,'description',NEW.DESCRIPTION) "
        "FROM EVENT_CONTEXT WHERE ID=1; END",
        "CREATE TRIGGER AUDIT_DOCUMENT_UPDATE AFTER UPDATE ON DOCUMENT BEGIN "
        "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,"
        "OBJECT_ID,BEFORE_JSON,AFTER_JSON) SELECT AUTHOR,EVENT_GROUP,'UPDATE',"
        "'DOCUMENT',NEW.ID,json_object('title',OLD.TITLE,'description',OLD."
        "DESCRIPTION),json_object('title',NEW.TITLE,'description',NEW."
        "DESCRIPTION) FROM EVENT_CONTEXT WHERE ID=1; END",
        "CREATE TRIGGER AUDIT_DOCUMENT_DELETE AFTER DELETE ON DOCUMENT BEGIN "
        "INSERT INTO EVENT_LOG(AUTHOR,EVENT_GROUP,EVENT_TYPE,OBJECT_TYPE,"
        "OBJECT_ID,BEFORE_JSON) SELECT AUTHOR,EVENT_GROUP,'DELETE','DOCUMENT',"
        "OLD.ID,json_object('title',OLD.TITLE,'description',OLD.DESCRIPTION) "
        "FROM EVENT_CONTEXT WHERE ID=1; END",
        "INSERT INTO MIGRATION_REPORT(FROM_VERSION,TO_VERSION,REPORT) VALUES("
        "17,18,'Nettoyage v18 terminé : modèle normalisé activé et structures "
        "historiques supprimées, MODEL fusionné dans CONFIGURATION.')"};
    for (const QString &statement : v18) {
      if (!hasLegacyRequirement &&
          (statement.contains("R.PARENT_ID") || statement.contains("R.PT_ID") ||
           statement.contains("SELECT PARENT_ID,ID") ||
           statement.contains("SELECT ID,PT_ID,1 FROM REQUIREMENT") ||
           statement.contains("SELECT ID,VERIF_METHOD") ||
           statement.contains("Document recréé depuis") ||
           statement == documentNodeBackfill))
        continue;
      if (!hasLegacyDocumentReference &&
          (statement.contains("D.REFERENCE") ||
           statement.contains("SELECT ID,REFERENCE,1 FROM DOCUMENT")))
        continue;
      if (!hasLegacyChanges && statement.contains("CONF_CHANGE"))
        continue;
      if (!hasLegacyLogs && statement.contains("FROM REQ_LOGS"))
        continue;
      if (!hasLegacyModel &&
          (statement.contains("MODEL") || statement.contains("PART_V18") ||
           statement == "DROP TABLE PART" ||
           statement == "DROP TABLE MODEL"))
        continue;
      if (!execute(db, statement, errorMessage)) {
        db.rollback();
        return false;
      }
    }
    QSqlQuery check("PRAGMA foreign_key_check", db);
    if (!check.exec() || check.next()) {
      if (errorMessage) {
        *errorMessage = check.lastError().text().isEmpty()
            ? QString("La migration v18 produit une clé étrangère invalide "
                      "dans %1 (ligne %2, table cible %3).").arg(
                  check.value(0).toString(), check.value(1).toString(),
                  check.value(2).toString())
            : check.lastError().text();
      }
      db.rollback();
      return false;
    }
  }

  if (!execute(db, QString("PRAGMA user_version=%1").arg(CurrentVersion),
               errorMessage) ||
      !db.commit()) {
    if (errorMessage && errorMessage->isEmpty())
      *errorMessage = db.lastError().text();
    db.rollback();
    return false;
  }
  return true;
}
