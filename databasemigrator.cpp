#include "databasemigrator.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
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
  if (version > CurrentVersion) {
    if (errorMessage)
      *errorMessage = QString("Cette base utilise le schéma %1, plus récent "
                              "que le schéma %2 pris en charge.")
                          .arg(version)
                          .arg(CurrentVersion);
    return false;
  }

  if (!db.transaction()) {
    if (errorMessage)
      *errorMessage = db.lastError().text();
    return false;
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
      << "INSERT OR IGNORE INTO REQUIREMENT_PT(REQ_ID,PT_ID) SELECT ID,PT_ID "
         "FROM REQUIREMENT WHERE PT_ID IS NOT NULL"
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
      << "CREATE INDEX IF NOT EXISTS IDX_REQ_REL_SOURCE ON "
         "REQUIREMENT_RELATION(SOURCE_REQ_ID)"
      << "CREATE INDEX IF NOT EXISTS IDX_REQ_REL_TARGET ON "
         "REQUIREMENT_RELATION(TARGET_REQ_ID)"
      << "CREATE INDEX IF NOT EXISTS IDX_REQ_VERIFICATION_REQ ON "
         "REQUIREMENT_VERIFICATION(REQ_ID,POSITION)"
      << "CREATE INDEX IF NOT EXISTS IDX_DOC_NODE_DOC ON "
         "DOCUMENT_NODE(DOC_ID,PARENT_ID,POSITION)"
      << "CREATE INDEX IF NOT EXISTS IDX_EVENT_OBJECT ON "
         "EVENT_LOG(OBJECT_TYPE,OBJECT_ID,EVENT_TIME)";

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
         "NEW.VERSION,'path',NEW.FILE_PATH,'sha256',NEW.FILE_SHA256)); END";

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
      {"DOCUMENT", "METADATA_JSON", "TEXT"},
      {"INTERFACE", "CODE", "TEXT"},
      {"INTERFACE", "STATUS", "TEXT NOT NULL DEFAULT 'DRAFT'"},
      {"PT", "SEGMENT", "TEXT"},
      {"PT", "DESCRIPTION", "TEXT"},
      {"PT", "POSITION", "INTEGER NOT NULL DEFAULT 0"},
      {"PT", "ARCHIVED", "INTEGER NOT NULL DEFAULT 0 CHECK(ARCHIVED IN (0,1))"},
      {"PT", "CREATED_AT", "TEXT"},
      {"PT", "UPDATED_AT", "TEXT"},
      {"REQUIREMENT_PT", "IS_PRIMARY",
       "INTEGER NOT NULL DEFAULT 0 CHECK(IS_PRIMARY IN (0,1))"},
      {"REQUIREMENT_VERIFICATION", "VERIFICATION_LEVEL_PT_ID",
       "INTEGER REFERENCES PT(ID)"}};
  for (const Column &column : columns) {
    if (!addColumnIfMissing(db, column.table, column.name, column.definition,
                            errorMessage)) {
      db.rollback();
      return false;
    }
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
               "UPDATE REQUIREMENT_PT SET IS_PRIMARY=1 WHERE PT_ID=(SELECT "
               "R.PT_ID FROM REQUIREMENT R WHERE R.ID=REQUIREMENT_PT.REQ_ID) "
               "AND NOT EXISTS(SELECT 1 FROM REQUIREMENT_PT P WHERE "
               "P.REQ_ID=REQUIREMENT_PT.REQ_ID AND P.IS_PRIMARY=1)",
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
    if ((hasLegacyChapters && !execute(db,
                 "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,"
                 "NODE_TYPE,POSITION,TITLE) SELECT C.DOC_ID,NULL,'CHAPTER',"
                 "1000000+C.ID,C.CHAPTER FROM REQ_CHAPTER C",
                 errorMessage)) ||
        !execute(db, hasLegacyChapters ?
                 "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,"
                 "NODE_TYPE,POSITION,REQ_ID) SELECT R.DOC_ID,(SELECT N.ID "
                 "FROM DOCUMENT_NODE N JOIN REQ_CHAPTER C ON C.DOC_ID=N.DOC_ID "
                 "AND C.CHAPTER=N.TITLE WHERE C.ID=R.DOC_CHAPTER AND "
                 "N.NODE_TYPE='CHAPTER' LIMIT 1),'REQUIREMENT',2000000+R.ID,R.ID FROM "
                 "REQUIREMENT R WHERE R.DOC_ID IS NOT NULL" :
                 "INSERT OR IGNORE INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,"
                 "NODE_TYPE,POSITION,REQ_ID) SELECT R.DOC_ID,NULL,'REQUIREMENT',"
                 "2000000+R.ID,R.ID FROM REQUIREMENT R WHERE R.DOC_ID IS NOT NULL",
                 errorMessage)) {
      db.rollback();
      return false;
    }
  }

  // Preserve the legacy single-parent relationship as an explicit decomposition
  // link.
  if (!execute(
          db,
          "INSERT OR IGNORE INTO "
          "REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID,COMMENT) "
          "SELECT PARENT_ID,ID,1,'Relation migrée depuis PARENT_ID' FROM "
          "REQUIREMENT WHERE PARENT_ID IS NOT NULL",
          errorMessage) ||
      !execute(db, QString("PRAGMA user_version=%1").arg(CurrentVersion),
               errorMessage) ||
      !db.commit()) {
    if (errorMessage && errorMessage->isEmpty())
      *errorMessage = db.lastError().text();
    db.rollback();
    return false;
  }
  return true;
}
