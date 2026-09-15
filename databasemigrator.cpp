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
  const bool foreignKeys = [&db] {
    QSqlQuery q("PRAGMA foreign_keys", db);
    return q.next() && q.value(0).toBool();
  }();
  struct ForeignKeyRestore {
    QSqlDatabase db;
    bool restore;
    ~ForeignKeyRestore() {
      if (restore) {
        QSqlQuery q(db);
        q.exec("PRAGMA foreign_keys=ON");
      }
    }
  } restore{db, rebuildInterface && foreignKeys};
  if (rebuildInterface && foreignKeys) {
    QSqlQuery q(db);
    q.exec("PRAGMA foreign_keys=OFF");
  }

  if (!db.transaction()) {
    if (errorMessage)
      *errorMessage = db.lastError().text();
    return false;
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
         "('CHANGE_REQUEST','Change Request'),('WAIVER','Waiver'),"
         "('DEVIATION','Dérogation'),('OTHER','Autre')"
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
    if (!addColumnIfMissing(db, column.table, column.name, column.definition,
                            errorMessage)) {
      db.rollback();
      return false;
    }
  }

  // Le schéma historique stockait type et statut comme textes contraints.
  // Les colonnes restent présentes pour la compatibilité, tandis que les
  // catalogues deviennent la source de vérité du registre des changements.
  if (!execute(db,
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
               errorMessage) ||
      !execute(db,
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
  if (!execute(db,
               "INSERT OR IGNORE INTO "
               "INTERFACE_DOCUMENT(INTERFACE_ID,DOC_ID,CHAPTER_NODE_ID) SELECT "
               "ID,DOC_ID,NULL FROM INTERFACE WHERE DOC_ID IS NOT NULL",
               errorMessage)) {
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
               "UPDATE REQUIREMENT_PT SET IS_PRIMARY=1 WHERE PT_ID=(SELECT "
               "R.PT_ID FROM REQUIREMENT R WHERE R.ID=REQUIREMENT_PT.REQ_ID) "
               "AND NOT EXISTS(SELECT 1 FROM REQUIREMENT_PT P WHERE "
               "P.REQ_ID=REQUIREMENT_PT.REQ_ID AND P.IS_PRIMARY=1)",
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
