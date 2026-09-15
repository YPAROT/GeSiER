#include <QAbstractItemModelTester>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>
#include <algorithm>

#include "databasemigrator.h"
#include "producttreeservice.h"
#include "req_sqlmanager.h"
#include "requirementservice.h"
#include "requirementrelationservice.h"
#include "documentservice.h"
#include "docxexportservice.h"
#include "applicabilityservice.h"
#include "verificationservice.h"
#include "interfaceservice.h"
#include "changeservice.h"
#include "xlsxreader.h"
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include "sqltreemodel.h"

namespace {
QString writeDocxTemplate(const QString &path) {
  const QByteArray document = R"(<?xml version="1.0" encoding="UTF-8"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><w:body><w:p><w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText> TOC \o "1-6" \h </w:instrText></w:r><w:r><w:fldChar w:fldCharType="separate"/></w:r><w:r><w:t>Sommaire existant</w:t></w:r><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p><w:p><w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText> DOCPROPERTY Title </w:instrText></w:r><w:r><w:fldChar w:fldCharType="separate"/></w:r><w:r><w:t>Ancien titre</w:t></w:r><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p><w:p><w:r><w:t>Texte générique avant</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val="Section3"/></w:pPr><w:r><w:t>3 Requirements</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_CONTENT}}</w:t></w:r></w:p><w:p><w:r><w:t>Texte générique après</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}</w:t></w:r></w:p><w:tbl><w:tblPr><w:tblStyle w:val="ReqTable"/></w:tblPr><w:tr><w:tc><w:p><w:r><w:t>{{REQ_CODE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_TITLE}}</w:t></w:r></w:p><w:p><w:r><w:t>{{REQ_DESCRIPTION}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Relations</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_RELATIONS}}</w:t></w:r></w:p></w:tc></w:tr></w:tbl><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_END}}</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val="Custom4"/><w:keepNext/></w:pPr><w:r><w:rPr><w:b/><w:color w:val="123456"/></w:rPr><w:t>{{GESIER_CHAPTER_LEVEL_4}}</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val="Custom5"/><w:ind w:left="720"/></w:pPr><w:r><w:rPr><w:i/></w:rPr><w:t>{{GESIER_CHAPTER_LEVEL_5}}</w:t></w:r></w:p><w:sectPr><w:headerReference w:type="default" r:id="rIdHeader"/></w:sectPr></w:body></w:document>)";
  const QByteArray styles = R"(<?xml version="1.0" encoding="UTF-8"?><w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:style w:type="paragraph" w:styleId="Section3"><w:pPr><w:outlineLvl w:val="2"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Custom4"><w:pPr><w:outlineLvl w:val="3"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Custom5"><w:pPr><w:outlineLvl w:val="4"/></w:pPr></w:style></w:styles>)";
  const QByteArray header = R"(<?xml version="1.0" encoding="UTF-8"?><w:hdr xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:p><w:r><w:t>{{GESIER_</w:t></w:r><w:r><w:t>REFERENCE}}</w:t></w:r></w:p></w:hdr>)";
  const QByteArray rels = R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rIdStyles" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/><Relationship Id="rIdHeader" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/header" Target="header1.xml"/></Relationships>)";
  const QByteArray rootRels = R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>)";
  const QByteArray types = R"(<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/><Override PartName="/word/header1.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml"/></Types>)";
  QZipWriter writer(path);
  writer.addFile("[Content_Types].xml", types);
  writer.addFile("_rels/.rels", rootRels);
  writer.addFile("word/document.xml", document);
  writer.addFile("word/styles.xml", styles);
  writer.addFile("word/header1.xml", header);
  writer.addFile("docProps/core.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>{{GESIER_DOCUMENT_TITLE}}</dc:title></cp:coreProperties>");
  writer.addFile("word/_rels/document.xml.rels", rels);
  writer.close();
  return path;
}
}

class DatabaseMigratorTest : public QObject {
  Q_OBJECT
  QTemporaryDir m_directory;
private slots:
  void migratesLegacySchemaWithoutLosingLinks();
  void storesMultipleVerificationMethods();
  void initializesNewProjectCatalogs();
  void managesRelationsAndDocumentOccurrences();
  void managesApplicabilityAndExportsMatrix();
  void managesVerificationMatrixAndExport();
  void managesInterfacesAndExportsN2();
  void managesChangesAndExportsRegister();
  void opensVersion11ProjectWithRequiredLegacyIcd();
};

void DatabaseMigratorTest::managesChangesAndExportsRegister() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("changes.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery q(db);
  QVERIFY(q.exec("INSERT INTO PT(ID,NAME,SEGMENT) VALUES(1,'System','SYS')"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
                 "VALUES(1,1,1,'Spécification système')"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,"
                 "STATUS,VERIF_METHOD) VALUES(1,1,'SYS-001',1,'Exigence "
                 "système',1,1,1)"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) "
                 "VALUES(1,1,1)"));
  ChangeService service(manager.currentConnection());
  auto custom = service.saveType(-1, "PROBLEM_REPORT", "Problem Report");
  QVERIFY2(custom.success, qPrintable(custom.message));
  auto decided = service.saveStatus(-1, "DECIDED", "Décidé", true, 35);
  QVERIFY2(decided.success, qPrintable(decided.message));
  ChangeRecord record;
  record.code = "PR-001";
  record.typeId = custom.id;
  record.statusId = decided.id;
  record.description = "Écart détecté en revue";
  record.links = {{"REQUIREMENT", "SYS-001", 1}};
  QVERIFY(!service.save(record).success); // un statut final impose une décision
  record.decision = "Accepté avec action de clôture";
  record.externalReference = "REDMINE-42";
  record.externalLink = "https://example.invalid/issues/42";
  auto saved = service.save(record);
  QVERIFY2(saved.success, qPrintable(saved.message));
  auto loaded = service.get(saved.id);
  QCOMPARE(loaded.typeCode, QString("PROBLEM_REPORT"));
  QCOMPARE(loaded.links.size(), 1);
  QCOMPARE(loaded.links[0].objectType, QString("REQUIREMENT"));
  ChangeFilter byRequirement;
  byRequirement.objectType = "REQUIREMENT";
  byRequirement.objectId = 1;
  QCOMPARE(service.find(byRequirement).size(), 1);
  ChangeFilter byPt;
  byPt.objectType = "PT";
  byPt.objectId = 1;
  QCOMPARE(service.find(byPt).size(), 1); // contexte déduit de l'exigence
  ChangeRecord invalid = loaded;
  invalid.links << ChangeLink{"PT", "System", 1};
  QVERIFY(!service.save(invalid).success);
  auto coverage = service.coverage();
  QCOMPARE(coverage.total, 1);
  QCOMPARE(coverage.complete, 1);
  QVERIFY(service.setArchived(saved.id, true).success);
  QCOMPARE(service.find({}).size(), 0);
  ChangeFilter archived;
  archived.includeArchived = true;
  QCOMPARE(service.find(archived).size(), 1);
  const QString path = directory.filePath("changes.xlsx");
  QVERIFY(service.exportXlsx(path, archived).success);
  QString error;
  const auto sheets = XlsxReader::read(path, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(sheets.size(), 3);
  QCOMPARE(sheets[0].name, QString("Synthèse"));
  QCOMPARE(sheets[1].name, QString("Changements"));
  QCOMPARE(sheets[2].name, QString("Associations"));
  QVERIFY(q.exec("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='CHANGE'"));
  QVERIFY(q.next());
  QVERIFY(q.value(0).toInt() >= 2);
  q = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

void DatabaseMigratorTest::opensVersion11ProjectWithRequiredLegacyIcd() {
  QTemporaryDir directory;
  const QString path = directory.filePath("version11.db");
  {
    REQ_SQLManager creator;
    const QSqlError creation = creator.newDB(path);
    QVERIFY2(creation.type() == QSqlError::NoError,
             qPrintable(creation.text()));
    creator.close();
  }
  {
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "version11-prep");
    db.setDatabaseName(path);
    QVERIFY(db.open());
    QSqlQuery q(db);
    QVERIFY(q.exec("PRAGMA foreign_keys=OFF"));
    QVERIFY(q.exec("CREATE TABLE INTERFACE_OLD(ID INTEGER PRIMARY KEY "
                   "AUTOINCREMENT,ELEMENT1 INTEGER NOT NULL,ELEMENT2 INTEGER "
                   "NOT NULL,DOC_ID INTEGER NOT NULL,DESCRIPTION TEXT,"
                   "DOC_CHAPTER INTEGER,CODE TEXT,STATUS TEXT NOT NULL DEFAULT "
                   "'DRAFT')"));
    QVERIFY(q.exec("DROP TABLE INTERFACE"));
    QVERIFY(q.exec("ALTER TABLE INTERFACE_OLD RENAME TO INTERFACE"));
    QVERIFY(q.exec("PRAGMA user_version=11"));
    q = QSqlQuery();
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("version11-prep");
  }
  REQ_SQLManager manager;
  const QSqlError opened = manager.openDB(path, false);
  QVERIFY2(opened.type() == QSqlError::NoError, qPrintable(opened.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery q("PRAGMA table_info(INTERFACE)", db);
  bool docIdIsNullable = false;
  while (q.next())
    if (q.value(1).toString() == "DOC_ID")
      docIdIsNullable = !q.value(3).toBool();
  QVERIFY(docIdIsNullable);
  q = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

void DatabaseMigratorTest::managesInterfacesAndExportsN2() {
  QTemporaryDir directory; REQ_SQLManager manager;
  const auto creation=manager.newDB(directory.filePath("interfaces.db"));
  QVERIFY2(creation.type()==QSqlError::NoError,qPrintable(creation.text()));
  QSqlDatabase db=QSqlDatabase::database(manager.currentConnection());
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1,"SYS").success);QSqlQuery q(db);QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));QVERIFY(q.next());int sys=q.value(0).toInt();
  QVERIFY(tree.addNode(sys,"PAYLOAD").success);QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='PAYLOAD'"));QVERIFY(q.next());int payload=q.value(0).toInt();
  QVERIFY(q.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,REFERENCE,TITLE) VALUES(1,%1,2,'ICD-001','ICD système')").arg(sys)));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,VERIF_METHOD) VALUES(1,%1,'REQ-IF',1,'Interface',1,1,1)").arg(sys)));
  InterfaceService service(manager.currentConnection());InterfaceRecord record;record.code="IF-SYS-PAY";record.element1Id=sys;record.element2Id=payload;record.description="Liaison bidirectionnelle";record.status="APPROVED";record.typeIds={1,2};record.requirementIds={1};record.documentIds={1};
  auto saved=service.save(record);QVERIFY2(saved.success,qPrintable(saved.message));
  InterfaceFilter reverse;reverse.ptId=payload;auto rows=service.find(reverse);QCOMPARE(rows.size(),1);QCOMPARE(rows[0].code,record.code);QVERIFY(rows[0].types.contains("Mécanique"));QVERIFY(rows[0].requirements.contains("REQ-IF"));QVERIFY(rows[0].documents.contains("ICD-001"));
  auto matrix=service.matrix();QCOMPARE(matrix.size(),1);QCOMPARE(matrix[0].interfaceCount,1);QCOMPARE(matrix[0].coveredCount,1);QCOMPARE(matrix[0].typeCount,2);
  QVERIFY(service.setArchived(saved.id,true).success);QCOMPARE(service.find({}).size(),0);reverse.includeArchived=true;QCOMPARE(service.find(reverse).size(),1);QCOMPARE(service.matrix().size(),0);
  QVERIFY(service.setArchived(saved.id,false).success);const QString path=directory.filePath("interfaces.xlsx");QVERIFY(service.exportXlsx(path,{}).success);QString error;auto sheets=XlsxReader::read(path,&error);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(sheets.size(),3);QCOMPARE(sheets[0].name,QString("Synthèse"));QCOMPARE(sheets[1].name,QString("Matrice N2"));QCOMPARE(sheets[2].name,QString("Interfaces"));
  QVERIFY(q.exec("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='INTERFACE'"));QVERIFY(q.next());QVERIFY(q.value(0).toInt()>=3);
  q=QSqlQuery();db=QSqlDatabase();manager.close();
}

void DatabaseMigratorTest::managesVerificationMatrixAndExport() {
  QTemporaryDir directory; REQ_SQLManager manager;
  const auto creation=manager.newDB(directory.filePath("verification-matrix.db"));
  QVERIFY2(creation.type()==QSqlError::NoError,qPrintable(creation.text()));
  QSqlDatabase db=QSqlDatabase::database(manager.currentConnection());
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1,"SYS").success);QSqlQuery q(db);QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));QVERIFY(q.next());const int pt=q.value(0).toInt();
  QVERIFY(q.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,1,'Vérification')").arg(pt)));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,VERIF_METHOD) VALUES(1,%1,'SYS-001',1,'Couverte',1,1,1)").arg(pt)));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,VERIF_METHOD) VALUES(2,%1,'SYS-002',1,'Manquante',1,1,1)").arg(pt)));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,%1,1),(2,%1,1)").arg(pt)));
  VerificationService service(manager.currentConnection());RequirementVerification v;v.methodId=1;v.levelPtId=pt;v.procedure="TC-001";v.redmine="https://redmine.invalid/1";v.means="Banc";v.verdict="PC";v.comment="Prévu";
  const auto saved=service.save(1,{v});QVERIFY2(saved.success,qPrintable(saved.message));
  auto rows=service.matrix({});QCOMPARE(rows.size(),2);QVERIFY(rows[0].covered);QVERIFY(!rows[1].covered);QCOMPARE(service.coverageRate(rows),50.0);
  RequirementVerification duplicate=v;QVERIFY(!service.save(1,{v,duplicate}).success);
  QVERIFY(tree.setArchived(pt,true).success);rows=service.matrix({});QVERIFY(!rows[0].covered);QVERIFY(rows[0].levels.contains("archivé"));
  const QString path=directory.filePath("verification.xlsx");QVERIFY(service.exportXlsx(path,{}).success);QString error;const auto sheets=XlsxReader::read(path,&error);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(sheets.size(),3);QCOMPARE(sheets[0].name,QString("Synthèse"));QCOMPARE(sheets[1].name,QString("Détails"));QCOMPARE(sheets[2].name,QString("Anomalies"));
}

void DatabaseMigratorTest::managesApplicabilityAndExportsMatrix() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const QSqlError creation = manager.newDB(directory.filePath("applicability.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "SYS").success);
  QSqlQuery q(db); QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'")); QVERIFY(q.next()); const int pt=q.value(0).toInt();
  QVERIFY(q.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,REFERENCE,TITLE) VALUES(1,%1,1,'SPEC','Specification')").arg(pt)));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,VERIF_METHOD) VALUES(1,%1,'REQ-A',1,'A',1,1,1),(2,%1,'REQ-B',1,'B',1,1,1)").arg(pt)));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,%1,1),(2,%1,1)").arg(pt)));
  ApplicabilityService service(manager.currentConnection());
  ConfigurationRecord c;c.code="FM";c.label="Flight Model";c.description="Modèle de vol";c.position=1;
  auto saved=service.saveConfiguration(c);QVERIFY2(saved.success,qPrintable(saved.message));
  ConfigurationRecord duplicate=c;duplicate.label="Duplicate";QVERIFY(!service.saveConfiguration(duplicate).success);
  QVERIFY(service.setApplicability(1,saved.id,-1,true,"base").success);
  QVERIFY(service.setApplicability(1,saved.id,-1,false,"mise à jour").success);
  QVERIFY(service.setApplicability(1,saved.id,pt,true,"exception PT").success);
  RequirementService requirements(manager.currentConnection());
  RequirementRecord edited=requirements.get(1);edited.typeId=1;edited.statusId=1;edited.title="A modifiée";QVERIFY(requirements.save(edited).success);
  QVERIFY(q.exec(QString("SELECT COUNT(*) FROM REQUIREMENT_APPLICABILITY WHERE REQ_ID=1 AND CONFIG_ID=%1 AND PT_ID=%2").arg(saved.id).arg(pt)));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);
  ApplicabilityFilter filter;filter.ptRootId=pt;const auto rows=service.matrix(filter);QCOMPARE(rows.size(),2);QCOMPARE(rows[0].cells.size(),1);QVERIFY(rows[0].cells[0].applicable);QCOMPARE(rows[0].cells[0].sourcePtId,pt);
  QCOMPARE(service.rate(rows),50.0);
  QVERIFY(service.setConfigurationActive(saved.id,false).success);
  QCOMPARE(service.configurations(true).size(),1);QVERIFY(!service.configurations(true)[0].active);QCOMPARE(service.configurations(true)[0].useCount,1);
  QCOMPARE(service.matrix(filter)[0].cells.size(),0);
  filter.includeArchivedConfigurations=true;
  QCOMPARE(service.matrix(filter)[0].cells.size(),1);
  const QString output=directory.filePath("matrix.xlsx");auto exported=service.exportXlsx(output,filter);QVERIFY2(exported.success,qPrintable(exported.message));QZipReader zip(output);QVERIFY(zip.exists());QVERIFY(!zip.fileData("xl/worksheets/sheet1.xml").isEmpty());QVERIFY(!zip.fileData("xl/worksheets/sheet2.xml").isEmpty());QVERIFY(!zip.fileData("xl/worksheets/sheet3.xml").isEmpty());
  QVERIFY(q.exec("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='CONFIGURATION'"));QVERIFY(q.next());QVERIFY(q.value(0).toInt()>=2);
  q=QSqlQuery();db=QSqlDatabase();manager.close();
}

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
  QVERIFY(documents.moveNode(first.id, -1, 0).success);
  const auto nested = documents.addChapter(1, first.id, "Sous-chapitre");
  QVERIFY(nested.success);

  DocumentService reopened(manager.currentConnection());
  const auto persisted = reopened.nodes(1);
  bool foundText=false, foundImage=false;
  QMap<int, QList<int>> positions;
  for(const auto &node:persisted){positions[node.parentId]<<node.position;foundText|=node.id==text.id&&node.textContent.contains("modifié");foundImage|=node.id==image.id&&node.imageLegend=="Vue logique";if(node.id==first.id)QCOMPARE(node.parentId,-1);if(node.id==nested.id)QCOMPARE(node.parentId,first.id);}
  QVERIFY(foundText); QVERIFY(foundImage);
  for(auto values:positions){std::sort(values.begin(),values.end());for(int i=0;i<values.size();++i)QCOMPARE(values[i],i);}
  QVERIFY(reopened.previewHtml(1).contains("Texte modifié"));
  const QString draftPath=directory.filePath("draft.docx");
  DocxExportService exporter(manager.currentConnection());
  DocxExportRequest draft;draft.documentId=1;draft.outputPath=draftPath;draft.draftLabel="Relecture interne";
  const auto draftResult=exporter.exportDocument(draft);QVERIFY2(draftResult.success,qPrintable(draftResult.message));
  QVERIFY(exporter.verifyHash(draftResult.id));
  QZipReader draftZip(draftPath);QVERIFY(draftZip.exists());QVERIFY(QString::fromUtf8(draftZip.fileData("word/document.xml")).contains("Texte modifié"));QVERIFY(QString::fromUtf8(draftZip.fileData("word/headerGesier.xml")).contains("DRAFT"));
  DocxExportRequest publication;publication.documentId=1;publication.outputPath=directory.filePath("publication.docx");publication.publication=true;publication.version="1.0";publication.title="Spécification publiée";publication.author="Test";
  const auto published=exporter.exportDocument(publication);QVERIFY2(published.success,qPrintable(published.message));QVERIFY(exporter.verifyHash(published.id));
  QVERIFY(query.exec(QString("SELECT LENGTH(SNAPSHOT_JSON),LENGTH(FILE_SHA256) FROM DOCUMENT_EXPORT WHERE ID=%1").arg(published.id)));QVERIFY(query.next());QVERIFY(query.value(0).toInt()>100);QCOMPARE(query.value(1).toInt(),64);
  QVERIFY(!query.exec(QString("UPDATE DOCUMENT_EXPORT SET TITLE='Altéré' WHERE ID=%1").arg(published.id)));
  QVERIFY(exporter.setGedInformation(published.id,"GED-001","https://ged.invalid/1").success);
  QVERIFY(!exporter.setGedInformation(draftResult.id,"GED-DRAFT",{}).success);
  QVERIFY(relations.add(1,3,3,"Interface critique").success);
  const QString templatePath=writeDocxTemplate(directory.filePath("template.docx"));
  const auto templateValidation=exporter.validateTemplate(templatePath);
  QVERIFY2(templateValidation.valid,qPrintable(templateValidation.errors.join('\n')));
  DocxExportRequest templated;templated.documentId=1;templated.templatePath=templatePath;templated.outputPath=directory.filePath("templated.docx");
  const auto templatedResult=exporter.exportDocument(templated);QVERIFY2(templatedResult.success,qPrintable(templatedResult.message));
  QZipReader templatedZip(templated.outputPath);const QString templatedDocument=QString::fromUtf8(templatedZip.fileData("word/document.xml"));const QString templatedHeader=QString::fromUtf8(templatedZip.fileData("word/header1.xml"));
  QXmlStreamReader documentReader(templatedDocument);while(!documentReader.atEnd())documentReader.readNext();QVERIFY2(!documentReader.hasError(),qPrintable(documentReader.errorString()));
  QXmlStreamReader headerReader(templatedHeader);while(!headerReader.atEnd())headerReader.readNext();QVERIFY2(!headerReader.hasError(),qPrintable(headerReader.errorString()));
  QVERIFY(templatedDocument.contains("Texte générique avant"));QVERIFY(templatedDocument.contains("Texte générique après"));QVERIFY(templatedDocument.contains("3 Requirements"));
  QVERIFY(templatedDocument.contains("Sommaire existant"));QVERIFY(templatedDocument.contains(" TOC "));
  QVERIFY(templatedDocument.contains("Specification"));QVERIFY(!templatedDocument.contains("Ancien titre"));
  QVERIFY(templatedDocument.contains("w:tblStyle w:val=\"ReqTable\""));QVERIFY(templatedDocument.contains("REQ-1"));QVERIFY(templatedDocument.contains("Dépend de REQ-3"));QVERIFY(templatedDocument.contains("Interface critique"));
  QVERIFY(templatedDocument.contains("w:pStyle w:val=\"Custom4\""));QVERIFY(templatedDocument.contains("w:pStyle w:val=\"Custom5\""));QVERIFY(!templatedDocument.contains("GESIER_CHAPTER_LEVEL"));QVERIFY(!templatedDocument.contains("GESIER_REQUIREMENT_TEMPLATE"));
  QVERIFY(templatedHeader.contains("SPEC-A"));QVERIFY(!templatedHeader.contains("GESIER_REFERENCE"));
  const QString settings=QString::fromUtf8(templatedZip.fileData("word/settings.xml"));QVERIFY(settings.contains("w:updateFields w:val=\"true\""));
  const QString coreProperties=QString::fromUtf8(templatedZip.fileData("docProps/core.xml"));QVERIFY(coreProperties.contains("<dc:title>Specification</dc:title>"));
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,REFERENCE,TITLE) VALUES(2,%1,1,'SPEC-B','Autre')").arg(root)));
  QVERIFY(documents.placeRequirement(2,-1,1).success);
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,REFERENCE,TITLE) VALUES(3,%1,1,'SPEC-C','Ordre')").arg(root)));
  const auto chapter1=documents.addChapter(3,-1,"Chapitre 1");
  const auto chapter2=documents.addChapter(3,-1,"Chapitre 2");
  const auto chapter3=documents.addChapter(3,-1,"Chapitre 3");
  const auto subchapter=documents.addChapter(3,chapter3.id,"Sous-chapitre 3.1");
  const auto trailingImage=documents.addImage(3,-1,QByteArray("IMAGE"),"Image finale");
  QVERIFY(chapter1.success);QVERIFY(chapter2.success);QVERIFY(chapter3.success);
  QVERIFY(subchapter.success);QVERIFY(trailingImage.success);
  QVERIFY(documents.moveNode(trailingImage.id,subchapter.id,0).success);
  QList<int> rootOrder;
  int imageParent=-1;
  for(const auto &node:DocumentService(manager.currentConnection()).nodes(3)){
    if(node.parentId<0)rootOrder<<node.id;
    if(node.id==trailingImage.id)imageParent=node.parentId;
  }
  QCOMPARE(rootOrder,QList<int>({chapter1.id,chapter2.id,chapter3.id}));
  QCOMPARE(imageParent,subchapter.id);
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
  QVERIFY(query.exec("SELECT CODE,LABEL FROM CHANGE_TYPE WHERE CODE IN "
                     "('CHANGE_REQUEST','DEVIATION','WAIVER') ORDER BY CODE"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toString(), QString("CHANGE_REQUEST"));
  QCOMPARE(query.value(1).toString(), QString("Demande de changement"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toString(), QString("DEVIATION"));
  QCOMPARE(query.value(1).toString(), QString("Déviation"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toString(), QString("WAIVER"));
  QCOMPARE(query.value(1).toString(), QString("Waiver / Dérogation"));
  query = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

QTEST_GUILESS_MAIN(DatabaseMigratorTest)
#include "database_migrator_test.moc"
