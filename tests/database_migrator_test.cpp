#include <QAbstractItemModelTester>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

#include "databasemigrator.h"
#include "producttreeservice.h"
#include "req_sqlmanager.h"
#include "requirementservice.h"
#include "requirementrelationservice.h"
#include "documentservice.h"
#include "sqltreemodel.h"

class DatabaseMigratorTest : public QObject {
  Q_OBJECT
  QTemporaryDir m_directory;
private slots:
  void migratesLegacySchemaWithoutLosingLinks();
  void storesMultipleVerificationMethods();
  void initializesNewProjectCatalogs();
  void managesRelationsAndDocumentOccurrences();
};

void DatabaseMigratorTest::managesRelationsAndDocumentOccurrences() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const QSqlError creation = manager.newDB(directory.filePath("relations.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "SYS").success);
  QSqlQuery query(db);
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'")); QVERIFY(query.next());
  const int root = query.value(0).toInt();
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,REFERENCE,TITLE) VALUES(1,%1,1,'SPEC-A','Specification')").arg(root)));
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,VERIF_METHOD) VALUES(1,%1,'REQ-1',1,'Parent',1,1,1),(2,%1,'REQ-2',1,'Child',1,1,1),(3,%1,'REQ-3',1,'Grandchild',1,1,1)").arg(root)));
  RequirementRelationService relations(manager.currentConnection());
  QVERIFY(relations.add(1, 2, 1).success);
  QVERIFY(!relations.add(1, 2, 1).success);
  QVERIFY(!relations.add(2, 1, 1).success);
  QVERIFY(relations.add(2, 3, 1, "niveau 2").success);
  QVERIFY(!relations.add(3, 1, 1).success);
  QVERIFY(relations.add(2, 1, 2).success); // sens inverse autorisé hors décomposition
  QVERIFY(!relations.add(1, 1, 3).success);
  const auto linked = relations.relations(2);
  QVERIFY(linked.size() >= 3);
  const int relationId = linked.first().id;
  QVERIFY(relations.updateComment(relationId, "commentaire modifié").success);
  QVERIFY(query.exec(QString("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='REQUIREMENT_RELATION' AND OBJECT_ID=%1 AND EVENT_TYPE='UPDATE'").arg(relationId)));
  QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(), 1);
  QCOMPARE(relations.relations(1).size(), 2);
  DocumentService documents(manager.currentConnection());
  const RequirementResult chapter = documents.addChapter(1, -1, "General");
  QVERIFY2(chapter.success, qPrintable(chapter.message));
  QVERIFY(documents.placeRequirement(1, chapter.id, 1).success);
  QVERIFY(documents.placeRequirement(1, -1, 1).success);
  int occurrences = 0;
  for (const DocumentNodeRecord &node : documents.nodes(1)) {
    if (node.requirementId == 1) {
      ++occurrences;
      QCOMPARE(node.requirementCode, QString("REQ-1"));
      QCOMPARE(node.requirementTitle, QString("Parent"));
    }
  }
  QCOMPARE(occurrences, 1);
  DocumentRecord saved = documents.document(1);
  saved.reference = "SPEC-A";
  saved.secondaryReferences = {"GED-42", "CUSTOMER-7"};
  saved.metadata["Auteur"] = "Équipe système";
  saved.metadata["Indice"] = "B";
  QVERIFY(documents.saveDocument(saved).success);
  QCOMPARE(documents.document(1).metadata.value("Indice"), QString("B"));
  QCOMPARE(documents.document(1).secondaryReferences.size(), 2);

  const auto first = documents.addChapter(1, -1, "Premier");
  const auto second = documents.addChapter(1, -1, "Second");
  QVERIFY(first.success); QVERIFY(second.success);
  const auto text = documents.addText(1, first.id, "<p><b>Texte riche</b></p>");
  const auto image = documents.addImage(1, first.id, QByteArray("PNG"), "Architecture");
  QVERIFY(text.success); QVERIFY(image.success);
  QVERIFY(documents.updateText(text.id, "<p>Texte modifié</p>").success);
  QVERIFY(documents.updateImage(image.id, QByteArray("NEWPNG"), "Vue logique").success);
  QVERIFY(documents.moveNode(first.id, second.id, 0).success);
  QVERIFY(!documents.moveNode(second.id, first.id, 0).success);

  DocumentService reopened(manager.currentConnection());
  const auto persisted = reopened.nodes(1);
  bool foundText=false, foundImage=false;
  QMap<int, QList<int>> positions;
  for(const auto &node:persisted){positions[node.parentId]<<node.position;foundText|=node.id==text.id&&node.textContent.contains("modifié");foundImage|=node.id==image.id&&node.imageLegend=="Vue logique";}
  QVERIFY(foundText); QVERIFY(foundImage);
  for(auto values:positions){std::sort(values.begin(),values.end());for(int i=0;i<values.size();++i)QCOMPARE(values[i],i);}
  QVERIFY(reopened.previewHtml(1).contains("Texte modifié"));
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,REFERENCE,TITLE) VALUES(2,%1,1,'SPEC-B','Autre')").arg(root)));
  QVERIFY(documents.placeRequirement(2,-1,1).success);
  QVERIFY(query.exec("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='DOCUMENT' AND EVENT_TYPE='COMPOSITION'"));
  QVERIFY(query.next()); QVERIFY(query.value(0).toInt() >= 7);
  query = QSqlQuery(); db = QSqlDatabase(); manager.close();
}

void DatabaseMigratorTest::migratesLegacySchemaWithoutLosingLinks() {
  QVERIFY(m_directory.isValid());
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "migration-test");
  db.setDatabaseName(m_directory.filePath("legacy.db"));
  QVERIFY2(db.open(), qPrintable(db.lastError().text()));
  QSqlQuery query(db);
  const QStringList legacy = {
      "CREATE TABLE PT(ID INTEGER PRIMARY KEY,NAME TEXT,PARENT INTEGER)",
      "CREATE TABLE REQ_STATUS(ID INTEGER PRIMARY KEY,SHORTCUT TEXT,STATUS "
      "TEXT)",
      "CREATE TABLE REQ_METHOD(ID INTEGER PRIMARY KEY,METHOD TEXT)",
      "CREATE TABLE REQ_TYPE(ID INTEGER PRIMARY KEY,TYPE TEXT)",
      "CREATE TABLE REQUIREMENT(ID INTEGER PRIMARY KEY,PT_ID INTEGER,CODE "
      "TEXT,DOC_ID INTEGER,DOC_CHAPTER INTEGER,TITLE TEXT,DESCRIPTION "
      "TEXT,TYPE INTEGER,STATUS INTEGER,SOURCE TEXT,PARENT_ID "
      "INTEGER,VERIF_LEVEL TEXT,VERIF_METHOD INTEGER,COMMENTS TEXT)",
      "CREATE TABLE DOCUMENT(ID INTEGER PRIMARY KEY,PT_ID INTEGER,TYPE "
      "INTEGER,TITLE TEXT,DESCRIPTION TEXT)",
      "CREATE TABLE INTERFACE(ID INTEGER PRIMARY KEY,ELEMENT1 INTEGER,ELEMENT2 "
      "INTEGER,DOC_ID INTEGER,DESCRIPTION TEXT,DOC_CHAPTER INTEGER)",
      "INSERT INTO PT VALUES(1,'SYS',NULL)",
      "INSERT INTO REQ_METHOD VALUES(1,'Analysis')",
      "INSERT INTO REQ_TYPE VALUES(1,'Custom legacy type')",
      "INSERT INTO "
      "REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,PARENT_ID,VERIF_METHOD) "
      "VALUES(1,1,'SYS-001',1,'Parent',NULL,1)",
      "INSERT INTO "
      "REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,PARENT_ID,VERIF_METHOD) "
      "VALUES(2,1,'SYS-002',1,'Enfant',1,1)"};
  for (const QString &sql : legacy)
    QVERIFY2(query.exec(sql), qPrintable(query.lastError().text()));
  QString error;
  QVERIFY2(DatabaseMigrator::migrate(db, &error), qPrintable(error));
  QVERIFY(query.exec("PRAGMA user_version"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), DatabaseMigrator::CurrentVersion);
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_RELATION WHERE "
                     "SOURCE_REQ_ID=1 AND TARGET_REQ_ID=2 AND TYPE_ID=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 1);
  QVERIFY(query.exec("CREATE TABLE EMPTY_PT(ID INTEGER PRIMARY KEY,NAME "
                     "TEXT,PARENT INTEGER)"));
  SqlTreeModel emptyTree("migration-test");
  emptyTree.setRelation(QSqlRelation("EMPTY_PT", "ID", "PARENT"));
  QAbstractItemModelTester modelTester(
      &emptyTree, QAbstractItemModelTester::FailureReportingMode::QtTest);
  QVERIFY(!emptyTree.select());
  QCOMPARE(emptyTree.rowCount(), 0);
  QVERIFY(
      query.exec("SELECT COUNT(*) FROM REQUIREMENT_PT WHERE REQ_ID IN (1,2)"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 2);
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_PT WHERE REQ_ID IN "
                     "(1,2) AND IS_PRIMARY=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 2);
  ProductTreeService pt("migration-test");
  QVERIFY(!pt.addNode(-1, "SECOND_ROOT").success);
  auto b = pt.addNode(1, "B");
  QVERIFY2(b.success, qPrintable(b.message));
  auto c = pt.addNode(1, "C");
  QVERIFY2(c.success, qPrintable(c.message));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='B'"));
  QVERIFY(query.next());
  const int bId = query.value(0).toInt();
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='C'"));
  QVERIFY(query.next());
  const int cId = query.value(0).toInt();
  auto d = pt.addNode(cId, "D");
  QVERIFY2(d.success, qPrintable(d.message));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='D'"));
  QVERIFY(query.next());
  const int dId = query.value(0).toInt();
  QCOMPARE(pt.fullCode(dId), QString("SYS-C-D"));
  QVERIFY(pt.moveNode(dId, bId, 0).success);
  QCOMPARE(pt.fullCode(dId), QString("SYS-B-D"));
  QVERIFY(!pt.moveNode(bId, dId, 0).success);
  QVERIFY(!pt.addNode(1, "B").success);
  auto blocked = pt.removeNode(1);
  QVERIFY(!blocked.success);
  QVERIFY(blocked.blockers.contains("Exigence SYS-001"));
  RequirementService requirements("migration-test");
  QCOMPARE(requirements.suggestCode(cId), QString("SYS-C-R-0001"));
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_VERIFICATION WHERE "
                     "REQ_ID IN (1,2) AND METHOD_ID=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 2);
  QVERIFY(query.exec("SELECT TYPE FROM REQ_TYPE WHERE ID=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toString(), QString("Custom legacy type"));
  QVERIFY(!query.exec(
      "INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID) "
      "VALUES(2,1,1)"));
  QVERIFY(
      query.exec("SELECT COUNT(*) FROM EVENT_LOG WHERE "
                 "OBJECT_TYPE='REQUIREMENT_RELATION' AND EVENT_TYPE='CREATE'"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 1);
  QVERIFY2(DatabaseMigrator::migrate(db, &error), qPrintable(error));
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_RELATION WHERE "
                     "SOURCE_REQ_ID=1 AND TARGET_REQ_ID=2 AND TYPE_ID=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 1);
  QVERIFY(query.exec("DELETE FROM REQUIREMENT_VERIFICATION WHERE REQ_ID=2"));
  QVERIFY2(DatabaseMigrator::migrate(db, &error), qPrintable(error));
  QVERIFY(query.exec(
      "SELECT COUNT(*) FROM REQUIREMENT_VERIFICATION WHERE REQ_ID=2"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 0);
  db.close();
}

void DatabaseMigratorTest::storesMultipleVerificationMethods() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  REQ_SQLManager manager;
  QSqlError creation = manager.newDB(directory.filePath("verification.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery query(db);
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "SYS").success);
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));
  QVERIFY(query.next());
  const int rootId = query.value(0).toInt();
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,1,'Verification')").arg(rootId)));
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,VERIF_METHOD) VALUES(1,%1,'SYS-001',1,'Requirement',1,1,1)").arg(rootId)));
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,%1,1)").arg(rootId)));
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT_VERIFICATION(REQ_ID,METHOD_ID,VERIFICATION_LEVEL_PT_ID,POSITION) VALUES(1,1,%1,0)").arg(rootId)));
  QVERIFY(query.exec(
      "INSERT OR IGNORE INTO REQ_METHOD(ID,METHOD) VALUES(2,'Test')"));
  RequirementService service(manager.currentConnection());
  RequirementRecord record = service.get(1);
  QCOMPARE(record.verifications.size(), 1);
  record.typeId = 1;
  record.statusId = 1;
  record.verifications[0].level = "System";
  record.verifications[0].levelPtId = rootId;
  record.verifications[0].procedure = "AN-001";
  record.verifications[0].verdict = "C";
  RequirementVerification test;
  test.methodId = 2;
  test.levelPtId = rootId;
  test.verdict = "PC";
  test.comment = "Niveau volontairement facultatif";
  record.verifications << test;
  RequirementResult saved = service.save(record);
  QVERIFY2(saved.success, qPrintable(saved.message));
  RequirementRecord reloaded = service.get(1);
  QCOMPARE(reloaded.verifications.size(), 2);
  QCOMPARE(reloaded.verifications[0].methodId, 1);
  QCOMPARE(reloaded.verifications[0].level, QString("System"));
  QCOMPARE(reloaded.verifications[1].methodId, 2);
  QCOMPARE(reloaded.verifications[1].verdict, QString("PC"));
  QVERIFY(
      query.exec("SELECT VERIF_METHOD,VERIF_LEVEL,VERIF_PROCEDURE,VERIF_STATUS "
                 "FROM REQUIREMENT WHERE ID=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 1);
  QCOMPARE(query.value(1).toString(), QString("SYS"));
  QCOMPARE(query.value(2).toString(), QString("AN-001"));
  QCOMPARE(query.value(3).toString(), QString("C"));
  RequirementFilter covered;
  covered.includeObsolete = true;
  covered.verified = 1;
  QCOMPARE(service.find(covered).size(), 1);
  RequirementFilter byMethod;
  byMethod.includeObsolete = true;
  byMethod.methodIds = {2};
  QCOMPARE(service.find(byMethod).size(), 1);
  RequirementFilter combined;
  combined.includeObsolete = true;
  combined.code = "SYS-001";
  combined.ptIds = {1};
  combined.methodIds = {2};
  combined.verified = 1;
  QCOMPARE(service.find(combined).size(), 1);
  QVERIFY(query.exec("INSERT INTO CONFIGURATION(ID,CODE,LABEL) "
                     "VALUES(1,'FM','Flight Model')"));
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT_APPLICABILITY(REQ_ID,CONFIG_ID,PT_ID) "
      "VALUES(1,1,1)"));
  RequirementFilter applicable;
  applicable.includeObsolete = true;
  applicable.configurationIds = {1};
  QCOMPARE(service.find(applicable).size(), 1);
  record.verifications << test;
  QVERIFY(!service.save(record).success);
  record.verifications.removeLast();
  record.verifications[0].verdict = "INVALID";
  QVERIFY(!service.save(record).success);
  query = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

void DatabaseMigratorTest::initializesNewProjectCatalogs() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  REQ_SQLManager manager;
  QSqlError error = manager.newDB(directory.filePath("new-project.db"));
  QVERIFY2(error.type() == QSqlError::NoError, qPrintable(error.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery query(db);
  QVERIFY(query.exec("SELECT COUNT(*),COUNT(DISTINCT CODE) FROM REQ_TYPE"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 14);
  QCOMPARE(query.value(1).toInt(), 14);
  QVERIFY(
      query.exec("SELECT GROUP_CONCAT(CODE,',') FROM REQ_TYPE ORDER BY ID"));
  QVERIFY(query.next());
  QVERIFY(query.value(0).toString().contains("PERF"));
  QVERIFY(query.value(0).toString().contains("ILS"));
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQ_STATUS"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 8);
  query = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

QTEST_GUILESS_MAIN(DatabaseMigratorTest)
#include "database_migrator_test.moc"
