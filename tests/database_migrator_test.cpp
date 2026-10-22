#include <QAbstractItemModelTester>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QtTest>
#include <algorithm>

#include "databasemigrator.h"
#include "auditservice.h"
#include "applicationlanguage.h"
#include "producttreeservice.h"
#include "req_sqlmanager.h"
#include "requirementservice.h"
#include "requirementrelationservice.h"
#include "documentservice.h"
#include "docxexportservice.h"
#include "docximportservice.h"
#include "applicabilityservice.h"
#include "verificationservice.h"
#include "interfaceservice.h"
#include "changeservice.h"
#include "coverageservice.h"
#include "traceabilitycontrolservice.h"
#include "xlsxreader.h"
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include "sqltreemodel.h"
#include "reqifservice.h"
#include "historyservice.h"
#include "referencedataservice.h"

namespace {
QString writeDocxTemplate(const QString &path) {
  const QByteArray document = R"(<?xml version="1.0" encoding="UTF-8"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><w:body><w:p><w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText> TOC \o "1-6" \h </w:instrText></w:r><w:r><w:fldChar w:fldCharType="separate"/></w:r><w:r><w:t>Sommaire existant</w:t></w:r><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p><w:p><w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText> DOCPROPERTY Title </w:instrText></w:r><w:r><w:fldChar w:fldCharType="separate"/></w:r><w:r><w:t>Ancien titre</w:t></w:r><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p><w:p><w:r><w:t>Texte générique avant</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val="Section3"/></w:pPr><w:r><w:t>3 Requirements</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_CONTENT}}</w:t></w:r></w:p><w:p><w:r><w:t>Texte générique après</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}</w:t></w:r></w:p><w:tbl><w:tblPr><w:tblStyle w:val="ReqTable"/></w:tblPr><w:tr><w:tc><w:p><w:r><w:t>{{REQ_CODE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_TITLE}}</w:t></w:r></w:p><w:p><w:r><w:t>{{REQ_DESCRIPTION}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Commentaires</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_COMMENTS}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Relations</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_RELATIONS}}</w:t></w:r></w:p></w:tc></w:tr></w:tbl><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_END}}</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val="Custom4"/><w:keepNext/></w:pPr><w:r><w:rPr><w:b/><w:color w:val="123456"/></w:rPr><w:t>{{GESIER_CHAPTER_LEVEL_4}}</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val="Custom5"/><w:ind w:left="720"/></w:pPr><w:r><w:rPr><w:i/></w:rPr><w:t>{{GESIER_CHAPTER_LEVEL_5}}</w:t></w:r></w:p><w:sectPr><w:headerReference w:type="default" r:id="rIdHeader"/></w:sectPr></w:body></w:document>)";
  const QByteArray styles = R"(<?xml version="1.0" encoding="UTF-8"?><w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:style w:type="paragraph" w:styleId="Section3"><w:pPr><w:outlineLvl w:val="2"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Custom4"><w:pPr><w:outlineLvl w:val="3"/></w:pPr></w:style><w:style w:type="paragraph" w:styleId="Custom5"><w:pPr><w:outlineLvl w:val="4"/></w:pPr></w:style></w:styles>)";
  const QByteArray header = R"(<?xml version="1.0" encoding="UTF-8"?><w:hdr xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:p><w:r><w:t>{{GESIER_</w:t></w:r><w:r><w:t>REFERENCE}}</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_META</w:t></w:r><w:r><w:t>DATA: Indice }}</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_METADATA:Absente}}</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_METADATA: AUTEUR }}</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_METADATA:}}</w:t></w:r></w:p></w:hdr>)";
  const QByteArray footer = R"(<?xml version="1.0" encoding="UTF-8"?><w:ftr xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:p><w:r><w:t>{{GESIER_METADATA:Auteur}}</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_METADATA}}</w:t></w:r></w:p></w:ftr>)";
  const QByteArray rels = R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rIdStyles" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/><Relationship Id="rIdHeader" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/header" Target="header1.xml"/></Relationships>)";
  const QByteArray rootRels = R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>)";
  const QByteArray types = R"(<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/><Override PartName="/word/header1.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml"/></Types>)";
  QZipWriter writer(path);
  writer.addFile("[Content_Types].xml", types);
  writer.addFile("_rels/.rels", rootRels);
  writer.addFile("word/document.xml", document);
  writer.addFile("word/styles.xml", styles);
  writer.addFile("word/header1.xml", header);
  writer.addFile("word/footer1.xml", footer);
  writer.addFile("docProps/core.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>{{GESIER_DOCUMENT_TITLE}}</dc:title></cp:coreProperties>");
  writer.addFile("word/_rels/document.xml.rels", rels);
  writer.close();
  return path;
}
QString writeImportFixture(const QString &path, const QByteArray &document,
                           const QByteArray &header = {},
                           const QByteArray &coreTitle = {},
                           const QByteArray &customReference = {}) {
  const QByteArray styles = R"(<?xml version="1.0" encoding="UTF-8"?><w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:style w:type="paragraph" w:styleId="Heading1"><w:pPr><w:outlineLvl w:val="0"/></w:pPr></w:style></w:styles>)";
  QZipWriter writer(path);
  writer.addFile("word/document.xml", document);
  writer.addFile("word/styles.xml", styles);
  if (!header.isEmpty()) writer.addFile("word/header1.xml", header);
  if (!coreTitle.isEmpty()) writer.addFile("docProps/core.xml", "<?xml version=\"1.0\"?><cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>" + coreTitle + "</dc:title></cp:coreProperties>");
  if (!customReference.isEmpty()) writer.addFile("docProps/custom.xml", "<?xml version=\"1.0\"?><Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/custom-properties\" xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\"><property name=\"Reference\"><vt:lpwstr>" + customReference + "</vt:lpwstr></property></Properties>");
  writer.close();
  return path;
}
QString writeStructuredDocxTemplate(const QString &path) {
  const QByteArray document = R"(<?xml version="1.0" encoding="UTF-8"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:p><w:r><w:t>{{GESIER_CONTENT}}</w:t></w:r></w:p><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}</w:t></w:r></w:p><w:tbl><w:tr><w:tc><w:p><w:r><w:t>{{REQ_CODE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_TITLE}}</w:t></w:r></w:p><w:p><w:r><w:t>{{REQ_DESCRIPTION}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Applicabilité</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_APPLICABILITY}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>{{GESIER_VERIFICATION_TEMPLATE_BEGIN}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>{{VERIF_METHOD}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{VERIF_LEVEL}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{VERIF_PROCEDURE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{VERIF_REDMINE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{VERIF_MEANS}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{VERIF_VERDICT}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{VERIF_COMMENT}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>{{GESIER_VERIFICATION_TEMPLATE_END}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Relations structurées</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>{{GESIER_RELATION_TEMPLATE_BEGIN}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>{{RELATION_TYPE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{RELATION_CODE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{RELATION_TITLE}}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{RELATION_COMMENT}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>{{GESIER_RELATION_TEMPLATE_END}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Agrégats</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_VERIFICATIONS}}</w:t></w:r></w:p><w:p><w:r><w:t>{{REQ_RELATIONS}}</w:t></w:r></w:p></w:tc></w:tr></w:tbl><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_END}}</w:t></w:r></w:p><w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:t>{{GESIER_CHAPTER_LEVEL_1}}</w:t></w:r></w:p><w:sectPr/></w:body></w:document>)";
  const QByteArray types = R"(<?xml version="1.0"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/></Types>)";
  QZipWriter writer(path); writer.addFile("[Content_Types].xml", types);
  writer.addFile("_rels/.rels", R"(<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>)");
  writer.addFile("word/document.xml", document);
  writer.addFile("word/_rels/document.xml.rels", R"(<?xml version="1.0"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"></Relationships>)");
  writer.close(); return path;
}
}

class DatabaseMigratorTest : public QObject {
  Q_OBJECT
  QTemporaryDir m_directory;
private slots:
  void createsNormalizedVersion24Schema();
  void configuresRequirementCodeFormat();
  void removesUnusedConfigurationsInheritedFromModel();
  void storesMarkdownImagesAtomically();
  void migratesLegacySchemaWithoutLosingLinks();
  void migratesRequirementUniquenessToCode();
  void rejectsCaseInsensitiveCodeCollisions();
  void storesMultipleVerificationMethods();
  void initializesNewProjectCatalogs();
  void managesRequirementReferenceData();
  void managesRelationsAndDocumentOccurrences();
  void managesApplicabilityAndExportsMatrix();
  void managesVerificationMatrixAndExport();
  void managesInterfacesAndExportsN2();
  void managesChangesAndExportsRegister();
  void opensVersion11ProjectWithRequiredLegacyIcd();
  void composesDocumentsInsideCallerTransaction();
  void createsDocumentsWithProductTreeRequirements();
  void importsExportsReqIfAndRejectsUnsafeXml();
  void calculatesDashboardCoverageFromOneService();
  void controlsHighLevelTraceability();
  void auditsDiagnosesBacksUpAndRestoresProject();
  void auditsConsolidatedRequirementDiffsWithoutImageBlobs();
  void previewsTaggedWordRoundTrip();
  void parsesSemanticWordTablesAndMetadata();
  void roundTripsStructuredDocxCollections();
};

void DatabaseMigratorTest::createsNormalizedVersion24Schema() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("normalized-v23.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery q(db);
  QVERIFY(q.exec("PRAGMA user_version"));
  QVERIFY(q.next());
  QCOMPARE(q.value(0).toInt(), 24);
  QVERIFY(db.tables().contains("EVENT_SUBJECT"));

  const auto columns = [&](const QString &table) {
    QSet<QString> names;
    QSqlQuery info(db);
    info.exec("PRAGMA table_info(" + table + ")");
    while (info.next()) names.insert(info.value(1).toString().toUpper());
    return names;
  };
  QVERIFY(columns("EVENT_LOG").contains("PAYLOAD_VERSION"));
  const QSet<QString> requirement = columns("REQUIREMENT");
  QVERIFY(requirement.contains("NOT_APPLICABLE"));
  for (const QString &removed : {"PARENT_ID", "PT_ID", "DOC_ID",
                                 "DOC_CHAPTER", "VERIF_METHOD",
                                 "VERIF_LEVEL", "VERIF_PROCEDURE",
                                 "REDMINE_REF", "VERIF_STATUS",
                                 "VERIF_MEANS"})
    QVERIFY2(!requirement.contains(removed), qPrintable(removed));
  QVERIFY(!columns("DOCUMENT").contains("REFERENCE"));
  QVERIFY(!columns("INTERFACE").contains("DOC_ID"));
  QVERIFY(!columns("INTERFACE").contains("DOC_CHAPTER"));
  QVERIFY(!columns("CHANGE_ITEM").contains("TYPE"));
  QVERIFY(!columns("CHANGE_ITEM").contains("STATUS"));
  for (const QString &removed : {"REQ_CHAPTER", "IF_CHAPTER", "REQ_LOGS",
                                 "REDMINE_ISSUES", "CONF_CHANGE", "CC_STATUS",
                                 "MODEL"})
    QVERIFY2(!db.tables().contains(removed), qPrintable(removed));
  QVERIFY(!db.tables().contains("PART"));
  QVERIFY(!db.tables().contains("IF_IMAGE"));
  QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master WHERE type='trigger' "
                 "AND name LIKE 'AUDIT_PT_%'"));
  QVERIFY(q.next());
  QCOMPARE(q.value(0).toInt(), 0);
  QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND "
                 "name IN('UQ_REQ_VERIFICATION_LEVEL',"
                 "'UQ_REQ_VERIFICATION_DRAFT')"));
  QVERIFY(q.next());
  QCOMPARE(q.value(0).toInt(), 2);
  QVERIFY(q.exec("ALTER TABLE REQUIREMENT DROP COLUMN NOT_APPLICABLE"));
  QVERIFY(q.exec("PRAGMA user_version=22"));
  QString migrationError;
  QVERIFY2(DatabaseMigrator::migrate(db, &migrationError),
           qPrintable(migrationError));
  QVERIFY(columns("REQUIREMENT").contains("NOT_APPLICABLE"));
  QVERIFY(q.exec("SELECT COUNT(*) FROM REQUIREMENT WHERE NOT_APPLICABLE<>0"));
  QVERIFY(q.next());
  QCOMPARE(q.value(0).toInt(), 0);
  QVERIFY(q.exec("PRAGMA foreign_key_check"));
  QVERIFY(!q.next());
  manager.close();
}

void DatabaseMigratorTest::configuresRequirementCodeFormat() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  QVERIFY(manager.newDB(directory.filePath("code-format.db")).type() == QSqlError::NoError);
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery migration(db);
  QVERIFY(migration.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(99,'LEGACY-CODE','Historique',1,1)"));
  QVERIFY(migration.exec("DELETE FROM PROJECT_META WHERE KEY LIKE 'requirement_code_%'"));
  QVERIFY(migration.exec("PRAGMA user_version=23"));
  QString migrationError; QVERIFY2(DatabaseMigrator::migrate(db,&migrationError),qPrintable(migrationError));
  QVERIFY(migration.exec("SELECT CODE FROM REQUIREMENT WHERE ID=99")); QVERIFY(migration.next());
  QCOMPARE(migration.value(0).toString(),QString("LEGACY-CODE"));
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "XIFU").success);
  QSqlQuery query(db);
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='XIFU'")); QVERIFY(query.next());
  const int root = query.value(0).toInt();
  QVERIFY(tree.addNode(root, "DRE").success);
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='DRE'")); QVERIFY(query.next());
  const int dre = query.value(0).toInt();
  RequirementService requirements(manager.currentConnection());
  QCOMPARE(requirements.suggestCode(dre), QString("XIFU-DRE-R-0000"));
  QVERIFY(requirements.validateCode("XIFU-DRE-R-0001", dre).conforming);
  QVERIFY(requirements.validateCode("XIFU-DRE-R-0000", dre).conforming);
  RequirementRecord first; first.code="XIFU-DRE-R-0000"; first.title="Première";
  first.typeId=1; first.statusId=1; first.primaryPtId=dre; first.ptIds={dre};
  QVERIFY(requirements.save(first).success);
  QCOMPARE(requirements.suggestCode(dre), QString("XIFU-DRE-R-0001"));

  ReferenceDataService settings(manager.currentConnection());
  RequirementCodeFormat format = settings.codeFormat();
  format.separator = '_'; format.marker = "REQ"; format.digits = 3;
  format.includeRoot = false;
  QVERIFY(settings.saveCodeFormat(format).success);
  QCOMPARE(tree.fullCode(dre), QString("XIFU_DRE"));
  QCOMPARE(requirements.suggestCode(dre), QString("DRE_REQ_001"));
  QVERIFY(requirements.validateCode("DRE_REQ_001", dre).conforming);
  const auto oldCode = requirements.validateCode("XIFU-DRE-R-0001", dre);
  QVERIFY(!oldCode.conforming); QVERIFY(!oldCode.errors.isEmpty());
  QCOMPARE(requirements.suggestCode(root), QString("REQ_000"));

  format.marker = "lower";
  QVERIFY(!settings.saveCodeFormat(format).success);
  format.marker = "REQ"; format.separator = 'A';
  QVERIFY(!settings.saveCodeFormat(format).success);
  manager.close();
}

void DatabaseMigratorTest::removesUnusedConfigurationsInheritedFromModel() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("configuration-v19.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery q(db);
  QVERIFY(q.exec("INSERT INTO PT(ID,NAME,SEGMENT) VALUES(1,'Système','SYS')"));
  QVERIFY(q.exec("INSERT INTO CONFIGURATION(ID,CODE,LABEL,DESCRIPTION) VALUES"
                 "(1,'PROTO','PROTO','Configuration migrée depuis MODEL'),"
                 "(2,'QM','QM','Configuration migrée depuis MODEL')"));
  QVERIFY(q.exec("CREATE TABLE PART(ID INTEGER PRIMARY KEY,PT_ID INTEGER NOT "
                 "NULL REFERENCES PT(ID),NAME TEXT NOT NULL UNIQUE,SERIAL "
                 "INTEGER NOT NULL UNIQUE,CONFIG_ID INTEGER NOT NULL "
                 "REFERENCES CONFIGURATION(ID),DESCRIPTION TEXT)"));
  QVERIFY(q.exec("INSERT INTO PART(PT_ID,NAME,SERIAL,CONFIG_ID) "
                 "VALUES(1,'EQ-1',1001,2)"));
  QVERIFY(q.exec("PRAGMA user_version=19"));
  QString migrationError;
  QVERIFY2(DatabaseMigrator::migrate(db, &migrationError),
           qPrintable(migrationError));
  QVERIFY(q.exec("SELECT CODE FROM CONFIGURATION ORDER BY CODE"));
  QVERIFY(q.next());
  QCOMPARE(q.value(0).toString(), QString("QM"));
  QVERIFY(!q.next());
  QVERIFY(!db.tables().contains("PART"));
  QVERIFY(q.exec("SELECT COUNT(*) FROM MIGRATION_REPORT WHERE TO_VERSION=20 "
                 "AND REPORT LIKE '%PROTO%'"));
  QVERIFY(q.next());
  QCOMPARE(q.value(0).toInt(), 1);
  manager.close();
}

void DatabaseMigratorTest::storesMarkdownImagesAtomically() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("markdown-images.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "SYS").success);
  QSqlQuery query(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));
  QVERIFY(query.next()); const int pt = query.value(0).toInt();
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(PT_ID,TYPE,TITLE) VALUES(%1,1,'Markdown')").arg(pt)));

  RequirementRecord record;
  record.code = "SYS-R-0001"; record.title = "Markdown";
  record.comments = "Première ligne accentuée\nDeuxième ligne";
  record.primaryPtId = pt; record.ptIds = {pt};
  RequirementImage image;
  image.token = "figure-1"; image.mimeType = "image/png";
  image.legend = "Architecture";
  image.data = QByteArray::fromBase64(
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=");
  record.images = {image};
  record.description = "**Texte**\n\n| A | B |\n| --- | --- |\n| 1 | 2 |\n\n![Architecture](gesier-image://figure-1)";
  auto saved = RequirementService(manager.currentConnection()).save(record);
  QVERIFY2(saved.success, qPrintable(saved.message));
  auto loaded = RequirementService(manager.currentConnection()).get(saved.id);
  QCOMPARE(loaded.description, record.description);
  QCOMPARE(loaded.comments, record.comments);
  QCOMPARE(loaded.images.size(), 1);
  QCOMPARE(loaded.images.first().data, image.data);
  QVERIFY(query.exec("SELECT ID FROM DOCUMENT ORDER BY ID LIMIT 1"));
  QVERIFY(query.next()); const int documentId = query.value(0).toInt();
  QVERIFY(DocumentService(manager.currentConnection())
              .placeRequirement(documentId, -1, saved.id).success);
  DocxExportRequest exportRequest;
  exportRequest.documentId = documentId;
  exportRequest.outputPath = directory.filePath("markdown.docx");
  const auto exported =
      DocxExportService(manager.currentConnection()).exportDocument(exportRequest);
  QVERIFY2(exported.success, qPrintable(exported.message));
  QZipReader archive(exportRequest.outputPath);
  const QString wordXml =
      QString::fromUtf8(archive.fileData("word/document.xml"));
  QVERIFY(wordXml.contains("<w:b/>"));
  QVERIFY(wordXml.contains("<w:tbl>"));
  QVERIFY(wordXml.contains("<w:drawing>"));
  QVERIFY(!archive.fileData("word/media/gesier1.png").isEmpty());

  loaded.description = "Image retirée";
  loaded.comments = "Commentaire modifié";
  QVERIFY(RequirementService(manager.currentConnection()).save(loaded).success);
  loaded = RequirementService(manager.currentConnection()).get(saved.id);
  QVERIFY(loaded.images.isEmpty());
  QCOMPARE(loaded.comments, QString("Commentaire modifié"));
  loaded.description = "![Absente](gesier-image://missing)";
  const auto rejected = RequirementService(manager.currentConnection()).save(loaded);
  QVERIFY(!rejected.success);
  QCOMPARE(RequirementService(manager.currentConnection()).get(saved.id).description,
           QString("Image retirée"));
  manager.close();
}

void DatabaseMigratorTest::controlsHighLevelTraceability() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("traceability.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery q(db);
  QVERIFY(q.exec("INSERT INTO PT(ID,PARENT,NAME,SEGMENT,ARCHIVED) VALUES"
                 "(1,NULL,'Système','SYS',0),(2,1,'Sous-système','SUB',0),"
                 "(3,NULL,'Archive','OLD',1)"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
                 "VALUES(1,1,1,'Spécification')"));
  QVERIFY(q.exec("INSERT INTO CONFIGURATION(CODE,LABEL,ACTIVE,POSITION) "
                 "VALUES('FM','Vol',1,1)"));
  QVERIFY(q.exec("INSERT INTO CONFIGURATION(CODE,LABEL,ACTIVE,POSITION) VALUES('OLD','Archivée',0,2)"));
  QVERIFY(q.exec("SELECT ID FROM CONFIGURATION WHERE CODE='FM'"));
  QVERIFY(q.next());
  const int flightConfigurationId = q.value(0).toInt();
  QVERIFY(q.exec("SELECT ID FROM CONFIGURATION WHERE CODE='OLD'"));
  QVERIFY(q.next());
  const int archivedConfigurationId = q.value(0).toInt();
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS,IS_TRACE_ROOT) VALUES"
                 "(1,'HL-1','Racine conforme',1,1,1),"
                 "(2,'LL-1','Enfant',1,1,0),"
                 "(3,'HL-2','Mauvais PT',1,1,1),"
                 "(4,'HL-OBS','Obsolète',1,8,1)"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
                 "(1,1,1),(2,2,1),(3,2,1),(4,1,1)"));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT_APPLICABILITY"
                         "(REQ_ID,CONFIG_ID,PT_ID,APPLICABLE) VALUES"
                         "(1,%1,NULL,1),(1,%1,1,0),(1,%2,NULL,1),"
                         "(3,%1,NULL,1)")
                     .arg(flightConfigurationId)
                     .arg(archivedConfigurationId)));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,"
                 "TYPE_ID) VALUES(1,2,1)"));

  TraceabilityControlService service(manager.currentConnection());
  QString error;
  auto rows = service.rows({}, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(rows.size(), 2); // l'obsolète est masquée
  QCOMPARE(rows[0].code, QString("HL-1"));
  QVERIFY(!rows[0].applicable); // l'exception root fausse prime
  QVERIFY(rows[0].rootConform);
  QVERIFY(rows[0].takenIntoAccount);
  QCOMPARE(rows[0].qualifyingRelationCount, 1);
  QCOMPARE(rows[1].code, QString("HL-2"));
  QVERIFY(rows[1].applicable);
  QVERIFY(!rows[1].rootConform);
  QVERIFY(!rows[1].takenIntoAccount);

  // La dépendance qualifie dans son sens métier : l'exigence aval dépend de
  // la racine. Le sens inverse ne doit pas compter.
  QVERIFY(q.exec("INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,"
                 "TYPE_ID) VALUES(2,1,3),(1,2,3),(1,2,2),(2,1,2)"));
  TraceabilityFilter applicable;
  applicable.applicable = 1;
  rows = service.rows(applicable, &error);
  QCOMPARE(rows.size(), 1);
  QCOMPARE(rows.first().code, QString("HL-2"));

  // Retirer l'exception rend HL-1 applicable et ses trois liens qualifiants
  // sont Décompose + Dérive de + Dépend de. Les liens inversés restent ignorés.
  QVERIFY(q.exec("DELETE FROM REQUIREMENT_APPLICABILITY WHERE REQ_ID=1 AND "
                 "CONFIG_ID=" + QString::number(flightConfigurationId) +
                 " AND PT_ID=1"));
  rows = service.rows({}, &error);
  QVERIFY(rows[0].applicable);
  QCOMPARE(rows[0].qualifyingRelationCount, 3);
  QVERIFY(rows[0].relationTypes.contains("Décompose"));
  QVERIFY(rows[0].relationTypes.contains("Dérive de"));
  QVERIFY(rows[0].relationTypes.contains("Dépend de"));

  TraceabilityFilter anomalies;
  anomalies.rootConform = 0;
  anomalies.takenIntoAccount = 0;
  anomalies.text = "mauvais";
  rows = service.rows(anomalies, &error);
  QCOMPARE(rows.size(), 1);
  QCOMPARE(rows.first().code, QString("HL-2"));

  TraceabilityFilter byStatus;
  byStatus.statusId = 1;
  QCOMPARE(service.rows(byStatus, &error).size(), 2);

  TraceabilityFilter includeObsolete;
  includeObsolete.includeObsolete = true;
  rows = service.rows(includeObsolete, &error);
  QCOMPARE(rows.size(), 3);
  QVERIFY(rows.last().obsolete);
  QVERIFY(rows.last().diagnostic.contains("contrôles désactivés"));

  RequirementService requirements(manager.currentConnection());
  RequirementRecord record = requirements.get(2, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  record.traceRoot = true;
  QVERIFY2(requirements.save(record).success, "Le marqueur doit être enregistré");
  QVERIFY(requirements.get(2).traceRoot);
}

void DatabaseMigratorTest::parsesSemanticWordTablesAndMetadata() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QByteArray model = R"(<?xml version="1.0"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Document: </w:t></w:r><w:r><w:t>{{GESIER_DOCUMENT_TITLE}}</w:t></w:r></w:p></w:tc></w:tr></w:tbl><w:p><w:r><w:t>{{GESIER_REQUIREMENT_</w:t></w:r><w:r><w:t>TEMPLATE_BEGIN}}</w:t></w:r></w:p><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Title:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_TITLE}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Reference:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Code {{REQ_</w:t></w:r><w:r><w:t>CODE}} end</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Description:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_DESCRIPTION}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Type:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_TYPE}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Status:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_STATUS}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Higher level req.:</w:t></w:r></w:p></w:tc><w:tc><w:p/></w:tc></w:tr></w:tbl><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_END}}</w:t></w:r></w:p></w:body></w:document>)";
  const QByteArray source = R"(<?xml version="1.0"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Document: Spécification importée</w:t></w:r></w:p></w:tc></w:tr></w:tbl><w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:t>Fonctions</w:t></w:r></w:p><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Title:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Détection de panne</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Reference:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Code SYS-R-0042 end</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Description:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:rPr><w:b/></w:rPr><w:t>Le système détecte la panne.</w:t></w:r></w:p><w:p><w:pPr><w:numPr/></w:pPr><w:r><w:t>Il journalise l'événement.</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Type:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Feature</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Status:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Draft</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Higher level req.:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>SYS-R-0001</w:t></w:r></w:p></w:tc></w:tr></w:tbl></w:body></w:document>)";
  const QByteArray modelHeader = R"(<w:hdr xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:p><w:r><w:t>Ref. {{GESIER_REFERENCE}}</w:t></w:r></w:p></w:hdr>)";
  const QByteArray sourceHeader = R"(<w:hdr xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:p><w:r><w:t>Ref. SPEC-0042</w:t></w:r></w:p></w:hdr>)";
  const QString modelPath = writeImportFixture(directory.filePath("semantic-model.docx"), model, modelHeader, "Titre propriété", "REF-PROPERTY");
  const QString sourcePath = writeImportFixture(directory.filePath("semantic-source.docx"), source, sourceHeader, "Titre de repli", "REF-DE-REPLI");
  DocxImportService service;
  const auto preview = service.preview(sourcePath, modelPath);
  QVERIFY2(preview.valid(), qPrintable(preview.errors.join('\n')));
  QCOMPARE(preview.documentTitle, QString("Spécification importée"));
  QCOMPARE(preview.documentReference, QString("SPEC-0042"));
  QCOMPARE(preview.requirements.size(), 1);
  const auto &requirement = preview.requirements.first();
  QCOMPARE(requirement.record.code, QString("SYS-R-0042"));
  QCOMPARE(requirement.record.title, QString("Détection de panne"));
  QCOMPARE(requirement.chapterPath, QStringList{"Fonctions"});
  QVERIFY(requirement.record.description.contains("**Le système détecte la panne.**"));
  QVERIFY(requirement.record.description.contains("Il journalise l'événement."));

  QByteArray nestedTableSource = source;
  nestedTableSource.replace(
      "<w:p><w:r><w:rPr><w:b/></w:rPr><w:t>Le système détecte la panne.</w:t></w:r></w:p><w:p><w:pPr><w:numPr/></w:pPr><w:r><w:t>Il journalise l'événement.</w:t></w:r></w:p>",
      "<w:p><w:r><w:t>Bits 7-5: Board Model</w:t></w:r></w:p>"
      "<w:tbl><w:tr><w:tc><w:p><w:r><w:rPr><w:b/></w:rPr><w:t>Code</w:t></w:r></w:p></w:tc>"
      "<w:tc><w:p><w:r><w:rPr><w:b/></w:rPr><w:t>Model</w:t></w:r></w:p></w:tc></w:tr>"
      "<w:tr><w:tc><w:p><w:r><w:t>000</w:t></w:r></w:p></w:tc>"
      "<w:tc><w:p><w:r><w:t>DM</w:t></w:r></w:p></w:tc></w:tr>"
      "<w:tr><w:tc><w:p><w:r><w:t>011</w:t></w:r></w:p></w:tc>"
      "<w:tc><w:p><w:r><w:t>STM</w:t></w:r></w:p></w:tc></w:tr></w:tbl>"
      "<w:p><w:r><w:t>Bits 4-0: Unique identifier</w:t></w:r></w:p>");
  const QString nestedTablePath = writeImportFixture(
      directory.filePath("nested-table-source.docx"), nestedTableSource,
      sourceHeader, "Titre de repli", "REF-DE-REPLI");
  const auto nestedTablePreview = service.preview(nestedTablePath, modelPath);
  QVERIFY2(nestedTablePreview.valid(),
           qPrintable(nestedTablePreview.errors.join('\n')));
  QCOMPARE(nestedTablePreview.requirements.size(), 1);
  const QString nestedDescription =
      nestedTablePreview.requirements.first().record.description;
  QVERIFY(nestedDescription.contains("Bits 7-5: Board Model"));
  QVERIFY(nestedDescription.contains("Code"));
  QVERIFY(nestedDescription.contains("STM"));
  QVERIFY(nestedDescription.contains("Bits 4-0: Unique identifier"));
  QCOMPARE(nestedTablePreview.requirements.first().type, QString("Feature"));
  QCOMPARE(nestedTablePreview.requirements.first().status, QString("Draft"));

  QByteArray formulaSource = source;
  formulaSource.replace(
      "<w:p><w:r><w:rPr><w:b/></w:rPr><w:t>Le système détecte la panne.</w:t></w:r></w:p><w:p><w:pPr><w:numPr/></w:pPr><w:r><w:t>Il journalise l'événement.</w:t></w:r></w:p>",
      "<w:p><w:r><w:t>Rapport </w:t></w:r><m:oMath>"
      "<m:f><m:num><m:r><m:t>a</m:t></m:r></m:num>"
      "<m:den><m:r><m:t>b</m:t></m:r></m:den></m:f>"
      "</m:oMath></w:p>");
  const QString formulaPath = writeImportFixture(
      directory.filePath("formula-source.docx"), formulaSource);
  const auto formulaPreview = service.preview(formulaPath, modelPath);
  QVERIFY2(formulaPreview.valid(), qPrintable(formulaPreview.errors.join('\n')));
  QVERIFY(formulaPreview.requirements.first().record.description.contains("$\\frac{a}{b}$"));
  QVERIFY(formulaPreview.requirements.first().diagnostics.isEmpty());

  QByteArray scriptsSource = formulaSource;
  scriptsSource.replace(
      "<m:f><m:num><m:r><m:t>a</m:t></m:r></m:num><m:den><m:r><m:t>b</m:t></m:r></m:den></m:f>",
      "<m:sSup><m:e><m:r><m:t>x</m:t></m:r></m:e>"
      "<m:sup><m:r><m:t>2</m:t></m:r></m:sup></m:sSup>"
      "<m:r><m:t> et </m:t></m:r>"
      "<m:sSub><m:e><m:r><m:t>T</m:t></m:r></m:e>"
      "<m:sub><m:r><m:t>max</m:t></m:r></m:sub></m:sSub>"
      "<m:r><m:t> ; tolérance </m:t></m:r>"
      "<m:sSubSup><m:sSubSupPr><m:ctrlPr><w:rPr><w:b/></w:rPr></m:ctrlPr></m:sSubSupPr>"
      "<m:e><m:argPr><m:argSz m:val=\"1\"/></m:argPr><m:r><m:t>10</m:t></m:r></m:e>"
      "<m:sub><m:r><m:t>-0.1</m:t></m:r></m:sub>"
      "<m:sup><m:r><m:t>+0.2</m:t></m:r></m:sup></m:sSubSup>"
      "<m:r><m:t>MH</m:t></m:r>"
      "<w:commentRangeStart w:id=\"1\"/><w:commentRangeEnd w:id=\"1\"/>"
      "<m:r><w:commentReference w:id=\"1\"/></m:r>"
      "<m:r><m:t>z</m:t></m:r>");
  const QString scriptsPath = writeImportFixture(
      directory.filePath("scripts-source.docx"), scriptsSource);
  const auto scriptsPreview = service.preview(scriptsPath, modelPath);
  QVERIFY2(scriptsPreview.valid(), qPrintable(scriptsPreview.errors.join('\n')));
  const QString scriptsDescription =
      scriptsPreview.requirements.first().record.description;
  QVERIFY(scriptsDescription.contains("{x}^{2}"));
  QVERIFY(scriptsDescription.contains("{T}_{max}"));
  QVERIFY(scriptsDescription.contains("{10}_{-0.1}^{+0.2}"));
  QVERIFY(scriptsDescription.contains("MHz"));
  QVERIFY(scriptsPreview.requirements.first().diagnostics.isEmpty());

  QByteArray unsupportedFormulaSource = formulaSource;
  unsupportedFormulaSource.replace("<m:f>", "<m:unknown>");
  unsupportedFormulaSource.replace("</m:f>", "</m:unknown>");
  const QString unsupportedFormulaPath = writeImportFixture(
      directory.filePath("unsupported-formula-source.docx"),
      unsupportedFormulaSource);
  const auto unsupportedFormulaPreview =
      service.preview(unsupportedFormulaPath, modelPath);
  QVERIFY2(unsupportedFormulaPreview.valid(),
           qPrintable(unsupportedFormulaPreview.errors.join('\n')));
  const auto &unsupportedRequirement =
      unsupportedFormulaPreview.requirements.first();
  QVERIFY(unsupportedRequirement.record.description.contains(
      QString::fromUtf8("Formule Word non convertie")));
  QVERIFY(!unsupportedRequirement.diagnostics.isEmpty());

  const QByteArray formulaCommentModel = R"(<?xml version="1.0"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math"><w:body><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}</w:t></w:r></w:p><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Title:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_TITLE}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Reference:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_CODE}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Description:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_DESCRIPTION}}</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Comment:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{REQ_COMMENTS}}</w:t></w:r></w:p></w:tc></w:tr></w:tbl><w:p><w:r><w:t>{{GESIER_REQUIREMENT_TEMPLATE_END}}</w:t></w:r></w:p></w:body></w:document>)";
  const QByteArray formulaCommentSource = R"(<?xml version="1.0"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math"><w:body><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Title:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Commentaire avec formule</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Reference:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>REQ-FORMULA-COMMENT</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Description:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Description valide</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Comment:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Rapport </w:t></w:r><m:oMath><m:f><m:num><m:r><m:t>x</m:t></m:r></m:num><m:den><m:r><m:t>y</m:t></m:r></m:den></m:f></m:oMath></w:p></w:tc></w:tr></w:tbl></w:body></w:document>)";
  const QString formulaCommentModelPath = writeImportFixture(
      directory.filePath("formula-comment-model.docx"), formulaCommentModel);
  const QString formulaCommentSourcePath = writeImportFixture(
      directory.filePath("formula-comment-source.docx"), formulaCommentSource);
  const auto formulaCommentPreview = service.preview(
      formulaCommentSourcePath, formulaCommentModelPath);
  QVERIFY2(formulaCommentPreview.valid(),
           qPrintable(formulaCommentPreview.errors.join('\n')));
  const auto &formulaCommentRequirement =
      formulaCommentPreview.requirements.first();
  QVERIFY(formulaCommentRequirement.commentsProvided);
  QVERIFY(formulaCommentRequirement.record.comments.contains("$\\frac{x}{y}$"));

  QByteArray fallbackModel = model;
  fallbackModel.replace("{{GESIER_DOCUMENT_TITLE}}", "Titre statique");
  const QString fallbackModelPath = writeImportFixture(directory.filePath("fallback-model.docx"), fallbackModel, {}, "Titre modèle");
  const QString fallbackSourcePath = writeImportFixture(directory.filePath("fallback-source.docx"), source, {}, "Titre propriété", "REF-PROPERTY");
  const auto fallback = service.preview(fallbackSourcePath, fallbackModelPath);
  QVERIFY2(fallback.valid(), qPrintable(fallback.errors.join('\n')));
  QCOMPARE(fallback.documentTitle, QString("Titre propriété"));
  QCOMPARE(fallback.documentReference, QString("REF-PROPERTY"));
  const QString noReferencePath = writeImportFixture(directory.filePath("filename-must-not-be-reference.docx"), source, {}, "Titre propriété");
  const auto noReference = service.preview(noReferencePath, fallbackModelPath);
  QVERIFY2(noReference.valid(), qPrintable(noReference.errors.join('\n')));
  QVERIFY(noReference.documentReference.isEmpty());

  const QByteArray tolerantSource = R"(<?xml version="1.0"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:t>Candidats</w:t></w:r></w:p><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Titre inhabituel:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Exigence exemple</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Reference:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Code EXAMPLE-1 end</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Description:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Bloc à vérifier</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Type:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Feature</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Status:</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Draft</w:t></w:r></w:p></w:tc></w:tr><w:tr><w:tc><w:p><w:r><w:t>Higher level req.:</w:t></w:r></w:p></w:tc><w:tc><w:p/></w:tc></w:tr></w:tbl><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Tableau documentaire</w:t></w:r></w:p></w:tc></w:tr></w:tbl></w:body></w:document>)";
  const QString tolerantPath = writeImportFixture(directory.filePath("tolerant-source.docx"), tolerantSource);
  const auto tolerant = service.preview(tolerantPath, modelPath);
  QVERIFY2(tolerant.valid(), qPrintable(tolerant.errors.join('\n')));
  QCOMPARE(tolerant.requirements.size(), 1);
  QCOMPARE(tolerant.requirements.first().recognition, DocxImportRecognition::Candidate);
  QVERIFY(!tolerant.requirements.first().selected);
  QCOMPARE(tolerant.requirements.first().record.code, QString("EXAMPLE-1"));
  QVERIFY(!tolerant.requirements.first().diagnostics.isEmpty());
}

void DatabaseMigratorTest::previewsTaggedWordRoundTrip() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("word-import.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery query(db);
  QVERIFY(query.exec("INSERT INTO PT(ID,NAME,SEGMENT,POSITION) VALUES(1,'Système','SYS',0)"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,1,1,'Word')"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SPEC-W',1)"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,DESCRIPTION,TYPE,STATUS,COMMENTS) VALUES(1,'SYS-R-0001','Détection','<p>Détecter la panne</p>',1,1,'Avis initial')"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,1,1)"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_NODE(ID,DOC_ID,PARENT_ID,NODE_TYPE,TITLE,POSITION) VALUES(1,1,NULL,'CHAPTER','Fonctions',0)"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,NODE_TYPE,POSITION,REQ_ID) VALUES(1,1,'REQUIREMENT',0,1)"));
  const QString model = writeDocxTemplate(directory.filePath("model.docx"));
  DocxExportRequest request; request.documentId = 1; request.templatePath = model;
  request.outputPath = directory.filePath("source.docx");
  DocxExportService exporter(manager.currentConnection());
  QVERIFY2(exporter.exportDocument(request).success, "L'export source doit réussir");
  DocxImportService importer(manager.currentConnection());
  const auto validation = importer.validateTemplate(model);
  QVERIFY2(validation.valid, qPrintable(validation.errors.join('\n')));
  const auto preview = importer.preview(request.outputPath, model);
  QVERIFY2(preview.valid(), qPrintable(preview.errors.join('\n')));
  QCOMPARE(preview.requirements.size(), 1);
  QCOMPARE(preview.requirements.first().record.code, QString("SYS-R-0001"));
  QCOMPARE(preview.requirements.first().record.title, QString("Détection"));
  QCOMPARE(preview.requirements.first().record.comments, QString("Avis initial"));
  QVERIFY(preview.requirements.first().commentsProvided);
  QCOMPARE(preview.documentTitle, QString("Word"));
  QCOMPARE(preview.documentReference, QString("SPEC-W"));
  QVERIFY(preview.requirements.first().record.description.contains("Détecter la panne"));
  QVERIFY(!preview.requirements.first().chapterPath.isEmpty());
  QCOMPARE(preview.requirements.first().chapterPath.last(), QString("Fonctions"));

  DocxImportPreview selective;
  DocxImportRequirement ignored; ignored.selected = false; ignored.ordinal = 1;
  ignored.record.code = "IGNORED-R-1"; ignored.record.title = "À ignorer"; ignored.record.description = "<p>Ignorée</p>";
  DocxImportRequirement imported; imported.selected = true; imported.ordinal = 2; imported.location = "Exigence 2 — Fonctions";
  imported.record.code = "SYS-R-0002"; imported.record.title = "Importée"; imported.record.description = "<p>Valide</p>";
  imported.record.traceRoot = true;
  imported.record.comments = "Commentaire Word";
  imported.commentsProvided = true;
  selective.requirements = {ignored, imported};
  DocxImportOptions importOptions; importOptions.documentId = 1; importOptions.primaryPtId = 1;
  QVERIFY2(importer.importPreview(selective, importOptions).success, "L'exigence cochée doit être importée");
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT WHERE CODE='IGNORED-R-1'")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(), 0);
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT WHERE CODE='SYS-R-0002'")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(), 1);
  QVERIFY(query.exec("SELECT IS_TRACE_ROOT FROM REQUIREMENT WHERE CODE='SYS-R-0002'")); QVERIFY(query.next()); QVERIFY(query.value(0).toBool());
  imported.record.traceRoot = false;
  imported.record.comments.clear();
  imported.commentsProvided = false;
  selective.requirements = {imported};
  QVERIFY2(importer.importPreview(selective, importOptions).success,
           "La mise à jour doit appliquer la valeur root importée");
  QVERIFY(query.exec("SELECT IS_TRACE_ROOT FROM REQUIREMENT WHERE CODE='SYS-R-0002'")); QVERIFY(query.next()); QVERIFY(!query.value(0).toBool());
  QVERIFY(query.exec("SELECT COMMENTS FROM REQUIREMENT WHERE CODE='SYS-R-0002'")); QVERIFY(query.next()); QCOMPARE(query.value(0).toString(), QString("Commentaire Word"));

  DocxImportPreview rollbackPreview;
  DocxImportRequirement beforeFailure; beforeFailure.selected = true; beforeFailure.ordinal = 3; beforeFailure.location = "Exigence 3";
  beforeFailure.record.code = "SYS-R-ROLLBACK-1"; beforeFailure.record.title = "Avant erreur"; beforeFailure.record.description = "<p>Valide</p>";
  DocxImportRequirement failure; failure.selected = true; failure.ordinal = 4; failure.location = "Exigence 4";
  failure.record.code = "SYS-R-ROLLBACK-2"; failure.record.title = "Erreur"; failure.record.description = "<p>Valide</p>"; failure.type = "TYPE-INCONNU";
  rollbackPreview.requirements = {beforeFailure, failure};
  QVERIFY(!importer.importPreview(rollbackPreview, importOptions).success);
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT WHERE CODE LIKE 'SYS-R-ROLLBACK-%'")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(), 0);
  manager.close();
}

void DatabaseMigratorTest::auditsDiagnosesBacksUpAndRestoresProject() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString project = directory.filePath("project.db");
  const QString backup = directory.filePath("manual-backup.db");
  REQ_SQLManager manager;
  QVERIFY(manager.newDB(project).type() == QSqlError::NoError);
  HistoryService history(manager.currentConnection());
  QVERIFY(history.setAuthor("Ingénieur test"));
  {
    QSqlDatabase db=QSqlDatabase::database(manager.currentConnection(),false);
    QSqlQuery q(db);
    ProductTreeService tree(manager.currentConnection());
    QVERIFY(tree.addNode(-1,"SYS").success);
    QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'")); QVERIFY(q.next());
    const int rootId=q.value(0).toInt();
    QVERIFY(tree.updateNode(rootId,"SYSTEME",{}).success);
    QVERIFY(tree.addNode(rootId,"SW").success);
    const auto events=history.find({},"PT",{});
    QCOMPARE(events.size(),3);
    const auto objectEvents=history.find({},"PT",{},rootId);
    QCOMPARE(objectEvents.size(),2);
    QCOMPARE(objectEvents.first().author,QString("Ingénieur test"));
    QVERIFY(objectEvents.first().beforeJson.contains("SYS"));
    QVERIFY(objectEvents.first().afterJson.contains("SYSTEME"));
  }
  const ProjectDiagnostic diagnostic=history.diagnose();
  QVERIFY2(diagnostic.ok,qPrintable(diagnostic.summary()));
  QCOMPARE(diagnostic.schemaVersion,DatabaseMigrator::CurrentVersion);
  QVERIFY(manager.manualBackup(backup));
  {
    QSqlQuery q(QSqlDatabase::database(manager.currentConnection(),false));
    QVERIFY(q.exec("UPDATE PT SET NAME='ALTERE' WHERE ID=1"));
  }
  QVERIFY(manager.restoreFromBackup(backup));
  {
    QSqlQuery q(QSqlDatabase::database(manager.currentConnection(),false));
    QVERIFY(q.exec("SELECT NAME FROM PT WHERE ID=1")); QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(),QString("SYSTEME"));
    QVERIFY(q.exec("PRAGMA foreign_key_check")); QVERIFY(!q.next());
  }
  manager.close();
}

void DatabaseMigratorTest::auditsConsolidatedRequirementDiffsWithoutImageBlobs() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  QVERIFY(manager.newDB(directory.filePath("audit-v19.db")).type()==QSqlError::NoError);
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1,"SYS").success);
  QSqlDatabase db=QSqlDatabase::database(manager.currentConnection());
  QSqlQuery q(db);QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));QVERIFY(q.next());const int pt=q.value(0).toInt();
  RequirementRecord r;r.code="SYS-R-0001";r.title="Titre initial";r.description="Texte initial\n\n![Schéma](gesier-image://img-a)";r.typeId=1;r.statusId=1;r.primaryPtId=pt;r.ptIds={pt};
  const QByteArray imageData=QByteArray::fromBase64("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=");
  r.images.append({"img-a","image/png","Schéma initial",imageData});
  RequirementService service(manager.currentConnection());const auto created=service.save(r);QVERIFY2(created.success,qPrintable(created.message));r.id=created.id;
  q.prepare("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='REQUIREMENT' AND OBJECT_ID=?");q.addBindValue(r.id);QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);
  r.title="Titre modifié";r.images[0].legend="Schéma révisé";QVERIFY(service.save(r).success);
  q.prepare("SELECT BEFORE_JSON,AFTER_JSON,PAYLOAD_VERSION FROM EVENT_LOG WHERE OBJECT_TYPE='REQUIREMENT' AND OBJECT_ID=? AND EVENT_TYPE='UPDATE' ORDER BY ID DESC LIMIT 1");q.addBindValue(r.id);QVERIFY(q.exec());QVERIFY(q.next());const QString before=q.value(0).toString(),after=q.value(1).toString();QCOMPARE(q.value(2).toInt(),2);QVERIFY(before.contains("Titre initial"));QVERIFY(after.contains("Titre modifié"));QVERIFY(after.contains("sha256"));QVERIFY(!after.contains(QString::fromLatin1(imageData.toBase64())));
  bool partial=true;QString plain;const QString html=HistoryService::formatDiff(before,after,&partial,&plain);QVERIFY(!partial);QVERIFY(html.contains("color:#b42318"));QVERIFY(html.contains("color:#067647"));QVERIFY(plain.contains("Titre modifié"));
  q.prepare("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='REQUIREMENT' AND OBJECT_ID=?");q.addBindValue(r.id);QVERIFY(q.exec());QVERIFY(q.next());const int count=q.value(0).toInt();QVERIFY(service.save(r).success);q.prepare("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='REQUIREMENT' AND OBJECT_ID=?");q.addBindValue(r.id);QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),count);
  q.prepare("SELECT COUNT(*) FROM EVENT_SUBJECT S JOIN EVENT_LOG E ON E.ID=S.EVENT_ID WHERE E.OBJECT_TYPE='REQUIREMENT' AND E.OBJECT_ID=? AND S.OBJECT_TYPE='PT' AND S.OBJECT_ID=?");q.addBindValue(r.id);q.addBindValue(pt);QVERIFY(q.exec());QVERIFY(q.next());QVERIFY(q.value(0).toInt()>=1);
  manager.close();
}

void DatabaseMigratorTest::calculatesDashboardCoverageFromOneService() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("coverage.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery q(db);
  QVERIFY(q.exec("INSERT INTO PT(ID,NAME,SEGMENT) VALUES(1,'System','SYS')"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
                 "VALUES(1,1,1,'Spécification')"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS,IS_TRACE_ROOT,NOT_APPLICABLE) VALUES"
                 "(1,'SYS-001','Couverte',1,1,1,0),"
                 "(2,'SYS-002','À compléter',2,2,0,0),"
                 "(3,'SYS-003','Hors périmètre',1,1,0,1)"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) "
                 "VALUES(1,1,1)"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT_NODE(DOC_ID,PARENT_ID,NODE_TYPE,TITLE,"
                 "POSITION,REQ_ID) VALUES"
                 "(1,NULL,'REQUIREMENT','Occurrence',1,1),"
                 "(1,NULL,'REQUIREMENT','Deuxième',2,2)"));
  QVERIFY(q.exec("DELETE FROM CONFIGURATION"));
  QVERIFY(q.exec("INSERT INTO CONFIGURATION(ID,CODE,LABEL,ACTIVE,POSITION) VALUES"
                 "(1,'ACTIVE','Active',1,1),(2,'ARCHIVE','Archivée',0,2)"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT_APPLICABILITY(REQ_ID,CONFIG_ID,PT_ID,"
                 "APPLICABLE) VALUES(1,1,NULL,1),(2,1,NULL,0),(2,2,NULL,1)"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT_EXPORT(DOC_ID,EXPORT_KIND,VERSION,TITLE,"
                 "AUTHOR,EXPORTED_AT) VALUES(1,'PUBLICATION','2.3',"
                 "'Spécification publiée','Auteur','2026-09-18 10:00:00')"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(2,1,1,'Document vide')"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SP-001',1)"));

  CoverageService service(manager.currentConnection());
  QString error;
  const auto metrics = service.metrics({}, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  auto metric = [&metrics](const QString &key) {
    return *std::find_if(metrics.cbegin(), metrics.cend(),
                         [&key](const CoverageMetric &m) { return m.key == key; });
  };
  QCOMPARE(metric("requirements").total, 3);
  QCOMPARE(metric("allocation").total, 2);
  QCOMPARE(metric("allocation").covered, 1);
  QCOMPARE(metric("allocation").percent(), 50);
  QCOMPARE(metric("traceability").total, 1); // la racine est hors dénominateur
  QCOMPARE(metric("documentation").covered, 2);
  QCOMPARE(metric("interfaces").total, 0);
  QCOMPARE(metric("interfaces").percent(), 0);
  QVERIFY(q.exec("SELECT COUNT(*) FROM DOCUMENT")); QVERIFY(q.next());
  QCOMPARE(q.value(0).toInt(), 2);
  const auto snapshot = service.snapshot({}, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(snapshot.requirements, 3);
  QCOMPARE(snapshot.applicableRequirements, 2);
  QCOMPARE(snapshot.documents, 2);
  QCOMPARE(snapshot.documentTypes.size(), 2);
  QCOMPARE(snapshot.documentTypes[0].code, QString("SP"));
  QCOMPARE(snapshot.documentTypes[0].value, 2);
  QCOMPARE(snapshot.documentTypes[1].code, QString("ICD"));
  QCOMPARE(snapshot.changeTypes.size(), 4);
  QCOMPARE(snapshot.changeTypes[0].label, QString("Demande de changement"));
  QCOMPARE(snapshot.changeTypes[1].label, QString("Waiver / Dérogation"));
  QCOMPARE(snapshot.changeTypes[2].label, QString("Déviation"));
  QCOMPARE(snapshot.changeStatuses.size(), 5);
  QCOMPARE(snapshot.documentRequirements.size(), 2);
  QCOMPARE(snapshot.documentRequirements[0].reference, QString("SP-001"));
  QCOMPARE(snapshot.documentRequirements[0].total, 2);
  QCOMPARE(snapshot.documentRequirements[0].applicable, 1); // archive ignorée
  QCOMPARE(snapshot.documentRequirements[0].percent(), 50);
  QCOMPARE(snapshot.documentRequirements[0].publicationVersion, QString("2.3"));
  QCOMPARE(snapshot.documentRequirements[1].reference, QString("—"));
  QCOMPARE(snapshot.documentRequirements[1].total, 0);

  CoverageScope filtered;
  filtered.search = "SYS-001";
  const auto searched = service.snapshot(filtered, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(searched.documentRequirements.size(), 2);
  QCOMPARE(searched.documentRequirements[0].total, 1);
  QCOMPARE(searched.documentRequirements[0].applicable, 1);

  filtered = {};
  filtered.configurationId = 1;
  const auto configured = service.snapshot(filtered, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(configured.documentRequirements[0].total, 2);
  QCOMPARE(configured.documentRequirements[0].applicable, 1);

  filtered = {};
  filtered.typeId = 2;
  filtered.statusIds = {2};
  const auto typedAndReviewed = service.snapshot(filtered, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(typedAndReviewed.documentRequirements[0].total, 1);
  QCOMPARE(typedAndReviewed.documentRequirements[0].applicable, 0);

  filtered = {};
  filtered.ptId = 1;
  const auto productTree = service.snapshot(filtered, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(productTree.documentRequirements[0].total, 1);
  QCOMPARE(productTree.documentRequirements[0].applicable, 1);

  filtered = {};
  filtered.documentId = 2;
  const auto emptyDocument = service.snapshot(filtered, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(emptyDocument.documentRequirements.size(), 1);
  QCOMPARE(emptyDocument.documentRequirements[0].total, 0);
  q = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

void DatabaseMigratorTest::importsExportsReqIfAndRejectsUnsafeXml() {
  QTemporaryDir directory; REQ_SQLManager manager;
  const auto creation=manager.newDB(directory.filePath("reqif.db"));QVERIFY2(creation.type()==QSqlError::NoError,qPrintable(creation.text()));
  QSqlDatabase db=QSqlDatabase::database(manager.currentConnection());QSqlQuery q(db);
  QVERIFY(q.exec("INSERT INTO PT(ID,NAME,SEGMENT) VALUES(1,'System','SYS')"));
  const QString input=directory.filePath("input.reqif");QFile file(input);QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(R"(<?xml version="1.0"?><REQ-IF><CORE-CONTENT><REQ-IF-CONTENT><DATATYPES/><SPEC-TYPES><SPEC-OBJECT-TYPE IDENTIFIER="T" LONG-NAME="Requirement"><SPEC-ATTRIBUTES><ATTRIBUTE-DEFINITION-STRING IDENTIFIER="A-CODE" LONG-NAME="Code"/><ATTRIBUTE-DEFINITION-STRING IDENTIFIER="A-DESC" LONG-NAME="Description"/><ATTRIBUTE-DEFINITION-STRING IDENTIFIER="A-COMMENT" LONG-NAME="Commentaire"/></SPEC-ATTRIBUTES></SPEC-OBJECT-TYPE></SPEC-TYPES><SPEC-OBJECTS><SPEC-OBJECT IDENTIFIER="EXT-1" LONG-NAME="Titre"><VALUES><ATTRIBUTE-VALUE-STRING THE-VALUE="SYS-001"><DEFINITION><ATTRIBUTE-DEFINITION-STRING-REF>A-CODE</ATTRIBUTE-DEFINITION-STRING-REF></DEFINITION></ATTRIBUTE-VALUE-STRING><ATTRIBUTE-VALUE-STRING THE-VALUE="Texte"><DEFINITION><ATTRIBUTE-DEFINITION-STRING-REF>A-DESC</ATTRIBUTE-DEFINITION-STRING-REF></DEFINITION></ATTRIBUTE-VALUE-STRING><ATTRIBUTE-VALUE-STRING THE-VALUE="À contrôler"><DEFINITION><ATTRIBUTE-DEFINITION-STRING-REF>A-COMMENT</ATTRIBUTE-DEFINITION-STRING-REF></DEFINITION></ATTRIBUTE-VALUE-STRING></VALUES></SPEC-OBJECT></SPEC-OBJECTS><SPEC-RELATIONS/><SPECIFICATIONS/></REQ-IF-CONTENT></CORE-CONTENT></REQ-IF>)");file.close();
  ReqIfService service(manager.currentConnection());ReqIfReport report=service.preview(input);QVERIFY2(report.valid(),qPrintable(report.errors.join('\n')));QCOMPARE(report.creates,1);QCOMPARE(report.objects.first().comments,QString("À contrôler"));QString error;QVERIFY2(service.importFile(input,report,&error),qPrintable(error));
  QVERIFY(q.exec("SELECT COUNT(*) FROM REQIF_IDENTITY WHERE EXTERNAL_ID='EXT-1'"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);
  const QString output=directory.filePath("output.reqif");QVERIFY2(service.exportFile(output,&error),qPrintable(error));QVERIFY(QFileInfo(output).size()>0);ReqIfReport roundTrip=service.preview(output);QVERIFY2(roundTrip.valid(),qPrintable(roundTrip.errors.join('\n')));
  QCOMPARE(roundTrip.objects.first().comments,QString("À contrôler"));
  QFile unsafe(directory.filePath("unsafe.reqif"));QVERIFY(unsafe.open(QIODevice::WriteOnly));unsafe.write("<!DOCTYPE x [<!ENTITY e SYSTEM 'file:///etc/passwd'>]><REQ-IF>&e;</REQ-IF>");unsafe.close();QVERIFY(!service.preview(unsafe.fileName()).valid());
  q=QSqlQuery();db=QSqlDatabase();manager.close();
}

void DatabaseMigratorTest::createsDocumentsWithProductTreeRequirements() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("document-create.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery query(db);
  QVERIFY(query.exec(
      "INSERT INTO PT(ID,PARENT,NAME,SEGMENT,POSITION,ARCHIVED) VALUES"
      "(101,NULL,'SYS','SYS',0,0),(102,101,'SYS-SUB','SUB',0,0),"
      "(103,102,'SYS-SUB-UNIT','UNIT',0,0),"
      "(104,101,'SYS-OLD','OLD',1,1)"));
  QVERIFY(query.exec(
      "INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(101,101,1,'Document source')"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(101,'SOURCE',1)"));
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES"
      "(101,'REQ-B','Directe',1,1),(102,'REQ-A','Secondaire racine',1,1),"
      "(103,'REQ-C','Petite-fille',1,1),(104,'REQ-D','Multi-PT',1,1),"
      "(105,'REQ-Z','Archivée',1,1)"));
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
      "(101,101,1),(102,102,1),(102,101,0),(103,103,1),"
      "(104,101,0),(104,102,1),(105,104,1)"));

  DocumentService service(manager.currentConnection());
  DocumentRecord exact;
  exact.ptId = 101;
  exact.typeId = 1;
  exact.reference = "EXACT";
  exact.title = "PT exact";
  auto result = service.createDocument(exact, {true, false});
  QVERIFY2(result.success, qPrintable(result.message));
  auto exactNodes = service.nodes(result.id);
  QCOMPARE(exactNodes.size(), 3);
  QCOMPARE(exactNodes[0].requirementCode, QString("REQ-A"));
  QCOMPARE(exactNodes[1].requirementCode, QString("REQ-B"));
  QCOMPARE(exactNodes[2].requirementCode, QString("REQ-D"));
  DocumentRecord updated = service.document(result.id);
  updated.description = "Modification sans resynchronisation";
  QVERIFY(service.saveDocument(updated).success);
  QCOMPARE(service.nodes(result.id).size(), 3);

  DocumentRecord recursive = exact;
  recursive.reference = "RECURSIVE";
  recursive.title = "Branche complète";
  result = service.createDocument(recursive, {true, true});
  QVERIFY2(result.success, qPrintable(result.message));
  const auto recursiveNodes = service.nodes(result.id);
  QCOMPARE(recursiveNodes.size(), 4);
  QCOMPARE(recursiveNodes[0].requirementCode, QString("REQ-A"));
  QCOMPARE(recursiveNodes[1].requirementCode, QString("REQ-B"));
  QCOMPARE(recursiveNodes[2].requirementCode, QString("REQ-C"));
  QCOMPARE(recursiveNodes[3].requirementCode, QString("REQ-D"));
  for (int i = 0; i < recursiveNodes.size(); ++i) {
    QCOMPARE(recursiveNodes[i].parentId, -1);
    QCOMPARE(recursiveNodes[i].position, i);
  }

  DocumentRecord empty = exact;
  empty.reference = "EMPTY";
  empty.title = "Document vide";
  result = service.createDocument(empty, {false, false});
  QVERIFY2(result.success, qPrintable(result.message));
  QVERIFY(service.nodes(result.id).isEmpty());

  QVERIFY(query.exec("SELECT COUNT(*) FROM DOCUMENT"));
  QVERIFY(query.next());
  const int beforeFailure = query.value(0).toInt();
  QVERIFY(query.exec("DROP TABLE EVENT_LOG"));
  DocumentRecord failing = exact;
  failing.reference = "ROLLBACK";
  failing.title = "Doit être annulé";
  result = service.createDocument(failing, {true, false});
  QVERIFY(!result.success);
  QVERIFY(query.exec("SELECT COUNT(*) FROM DOCUMENT"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), beforeFailure);
  manager.close();
}

void DatabaseMigratorTest::composesDocumentsInsideCallerTransaction() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("transaction.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery query(db);
  QVERIFY(query.exec("INSERT INTO PT(ID,NAME,SEGMENT) VALUES(1,'System','SYS')"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,1,1,'Spécification')"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SPEC-1',1)"));
  DocumentService documents(manager.currentConnection());
  QVERIFY(db.transaction());
  const auto chapter = documents.addChapter(1, -1, "Chapitre importé", false);
  QVERIFY2(chapter.success, qPrintable(chapter.message));
  QVERIFY(db.rollback());
  QVERIFY(query.exec("SELECT COUNT(*) FROM DOCUMENT_NODE WHERE DOC_ID=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 0);
  manager.close();
}

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
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'SYS-001','Exigence système',1,1)"));
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
  bool hasLegacyDocId = false;
  while (q.next())
    if (q.value(1).toString() == "DOC_ID")
      hasLegacyDocId = true;
  QVERIFY(!hasLegacyDocId);
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
  QVERIFY(q.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,2,'ICD système')").arg(sys)));
  QVERIFY(q.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'ICD-001',1)"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'REQ-IF','Interface',1,1)"));
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
  QVERIFY(tree.addNode(-1,"SYS").success);QSqlQuery q(db);QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));QVERIFY(q.next());const int pt=q.value(0).toInt();QVERIFY(tree.addNode(pt,"SUB").success);QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SUB'"));QVERIFY(q.next());const int sub=q.value(0).toInt();
  QVERIFY(q.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,1,'Vérification')").arg(pt)));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'SYS-001','Couverte / Draft',1,1),(2,'SYS-002','Draft',1,1),(3,'SYS-003','Manquante',1,1),(4,'SYS-004','Couverte',1,1)"));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,%1,1),(2,%1,1),(3,%1,1),(4,%1,1)").arg(pt)));
  VerificationService service(manager.currentConnection());RequirementVerification v;v.methodId=1;v.levelPtId=pt;v.procedure="TC-001";v.redmine="https://redmine.invalid/1";v.means="Banc";v.verdict="PC";v.comment="Prévu";
  RequirementVerification second=v;second.levelPtId=sub;second.procedure="TC-002";RequirementVerification draft=v;draft.levelPtId=-1;draft.procedure="À définir";
  const auto saved=service.save(1,{v,second,draft});QVERIFY2(saved.success,qPrintable(saved.message));
  QVERIFY(service.save(2,{draft}).success);QVERIFY(service.save(4,{v}).success);
  auto rows=service.matrix({});QCOMPARE(rows.size(),4);QCOMPARE(rows[0].coverageLabel(),QString("Couverte / Draft"));QCOMPARE(rows[1].coverageLabel(),QString("Draft"));QCOMPARE(rows[2].coverageLabel(),QString("Manquante"));QCOMPARE(rows[3].coverageLabel(),QString("Couverte"));QCOMPARE(service.coverageRate(rows),50.0);
  VerificationFilter draftFilter;draftFilter.coverage=4;QCOMPARE(service.matrix(draftFilter).size(),2);
  RequirementVerification duplicate=v;QVERIFY(!service.save(1,{v,duplicate}).success);
  QVERIFY(!service.save(1,{draft,draft}).success);
  const auto snapshot=CoverageService(manager.currentConnection()).snapshot();QCOMPARE(snapshot.verificationPlanning.covered,1);QCOMPARE(snapshot.verificationPlanning.coveredDraft,1);QCOMPARE(snapshot.verificationPlanning.draft,1);QCOMPARE(snapshot.verificationPlanning.missing,1);QCOMPARE(snapshot.verificationPlanning.planned(),3);
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
  QVERIFY(q.exec("DELETE FROM CONFIGURATION"));
  QVERIFY(q.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,1,'Specification')").arg(pt)));
  QVERIFY(q.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SPEC',1)"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'REQ-A','A',1,1),(2,'REQ-B','B',1,1)"));
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
  QVERIFY(service.setApplicability(2,saved.id,-1,true,"à supprimer").success);
  QVERIFY(service.setApplicability(2,saved.id,pt,false,"exception à supprimer").success);
  RequirementRecord excluded=requirements.get(2);excluded.notApplicable=true;
  QVERIFY(requirements.save(excluded).success);
  QCOMPARE(service.matrix(filter).size(),1);
  filter.includeNotApplicableRequirements=true;
  const auto excludedRows=service.matrix(filter);
  QVERIFY(excludedRows[1].notApplicable);
  QCOMPARE(service.rate(excludedRows),100.0);
  QVERIFY(!service.setApplicability(2,saved.id,-1,true).success);
  QVERIFY(q.exec("SELECT COUNT(*) FROM REQUIREMENT_APPLICABILITY WHERE REQ_ID=2"));
  QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);
  QCOMPARE(AuditService::requirementSnapshot(db,2).value("fields").toObject()
               .value("notApplicable").toInt(),1);
  excluded=requirements.get(2);excluded.notApplicable=false;
  QVERIFY(requirements.save(excluded).success);
  QVERIFY(!requirements.get(2).notApplicable);
  QCOMPARE(service.rate(service.matrix(filter)),50.0);
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS,NOT_APPLICABLE) VALUES"
                 "(3,'REQ-C','Obsolète',1,8,0),(4,'REQ-D','Obsolète N/A',1,8,1)"));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
                         "(3,%1,1),(4,%1,1)").arg(pt)));
  QCOMPARE(service.matrix(filter).size(),2);
  filter.includeObsoleteRequirements=true;
  const auto allStates=service.matrix(filter);
  QCOMPARE(allStates.size(),4);
  QVERIFY(!allStates[0].obsolete&&!allStates[0].notApplicable);
  QVERIFY(!allStates[1].obsolete&&!allStates[1].notApplicable);
  QVERIFY(allStates[2].obsolete&&!allStates[2].notApplicable);
  QVERIFY(allStates[3].obsolete&&allStates[3].notApplicable);
  QVERIFY(service.setConfigurationActive(saved.id,false).success);
  QCOMPARE(service.configurations(true).size(),1);QVERIFY(!service.configurations(true)[0].active);QCOMPARE(service.configurations(true)[0].useCount,1);
  QCOMPARE(service.matrix(filter)[0].cells.size(),0);
  filter.includeArchivedConfigurations=true;
  QCOMPARE(service.matrix(filter)[0].cells.size(),1);
  const QString output=directory.filePath("matrix.xlsx");auto exported=service.exportXlsx(output,filter);QVERIFY2(exported.success,qPrintable(exported.message));QZipReader zip(output);QVERIFY(zip.exists());QVERIFY(!zip.fileData("xl/worksheets/sheet1.xml").isEmpty());QVERIFY(!zip.fileData("xl/worksheets/sheet2.xml").isEmpty());QVERIFY(!zip.fileData("xl/worksheets/sheet3.xml").isEmpty());QString xlsxError;const auto sheets=XlsxReader::read(output,&xlsxError);QVERIFY2(xlsxError.isEmpty(),qPrintable(xlsxError));QCOMPARE(sheets[1].rows[0][6],QString("État"));QVERIFY(sheets[1].rows[0][7].contains("[ARCH]"));QCOMPARE(sheets[1].rows[3][6],QString("OBS"));QCOMPARE(sheets[1].rows[4][6],QString("OBS · N/A"));
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
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,1,'Specification')").arg(root)));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SPEC-A',1)"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'REQ-1','Parent',1,1),(2,'REQ-2','Child',1,1),(3,'REQ-3','Grandchild',1,1)"));
  RequirementRelationService relations(manager.currentConnection());
  QVERIFY(relations.add(1, 2, 1).success);
  QVERIFY(!relations.add(1, 2, 1).success);
  QVERIFY(!relations.add(2, 1, 1).success);
  QVERIFY(relations.add(2, 3, 1, "niveau 2").success);
  QVERIFY(!relations.add(3, 1, 1).success);
  QVERIFY(relations.add(2, 1, 2, "Choix d'architecture").success); // REQ-1 dérive de REQ-2
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
  QVERIFY(query.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) "
                     "VALUES(4,'REQ-OBS','Texte obsolète distinctif',1,"
                     "(SELECT ID FROM REQ_STATUS WHERE UPPER(SHORTCUT)='O' LIMIT 1))"));
  QVERIFY(documents.placeRequirement(1, -1, 4).success);
  bool foundActiveRequirement = false;
  bool foundObsoleteRequirement = false;
  for (const DocumentNodeRecord &node : documents.nodes(1)) {
    if (node.requirementId == 1) {
      foundActiveRequirement = true;
      QVERIFY(!node.requirementObsolete);
    } else if (node.requirementId == 4) {
      foundObsoleteRequirement = true;
      QVERIFY(node.requirementObsolete);
    }
  }
  QVERIFY(foundActiveRequirement);
  QVERIFY(foundObsoleteRequirement);
  DocumentRecord saved = documents.document(1);
  saved.reference = "SPEC-A";
  saved.secondaryReferences = {"GED-42", "CUSTOMER-7"};
  saved.metadata["Auteur"] = "Équipe système";
  saved.metadata["Indice"] = "B";
  saved.metadata["auteur"] = "Autre auteur";
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
  const QString preview = reopened.previewHtml(1);
  QVERIFY(preview.contains("Texte modifié"));
  QVERIFY(preview.contains("REQ-OBS — Texte obsolète distinctif [Obsolete]"));
  QVERIFY(preview.contains("<b style=\"color:#808080\">REQ-OBS"));
  QVERIFY(preview.contains("<b>REQ-1 — Parent</b>"));
  QVERIFY(!preview.contains("REQ-1 — Parent [Obsolete]"));
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
  QVERIFY(documents.placeRequirement(1, -1, 2).success);
  const QString templatePath=writeDocxTemplate(directory.filePath("template.docx"));
  const auto templateValidation=exporter.validateTemplate(templatePath,1);
  QVERIFY2(templateValidation.valid,qPrintable(templateValidation.errors.join('\n')));
  QVERIFY(templateValidation.warnings.join('\n').contains("absente",Qt::CaseInsensitive));
  QVERIFY(templateValidation.warnings.join('\n').contains("ambigu",Qt::CaseInsensitive));
  QVERIFY(templateValidation.warnings.join('\n').contains("aucun nom",Qt::CaseInsensitive));
  DocxExportRequest templated;templated.documentId=1;templated.templatePath=templatePath;templated.outputPath=directory.filePath("templated.docx");
  const auto templatedResult=exporter.exportDocument(templated);QVERIFY2(templatedResult.success,qPrintable(templatedResult.message));
  QZipReader templatedZip(templated.outputPath);const QString templatedDocument=QString::fromUtf8(templatedZip.fileData("word/document.xml"));const QString templatedHeader=QString::fromUtf8(templatedZip.fileData("word/header1.xml"));const QString templatedFooter=QString::fromUtf8(templatedZip.fileData("word/footer1.xml"));
  QXmlStreamReader documentReader(templatedDocument);while(!documentReader.atEnd())documentReader.readNext();QVERIFY2(!documentReader.hasError(),qPrintable(documentReader.errorString()));
  QXmlStreamReader headerReader(templatedHeader);while(!headerReader.atEnd())headerReader.readNext();QVERIFY2(!headerReader.hasError(),qPrintable(headerReader.errorString()));
  QVERIFY(templatedDocument.contains("Texte générique avant"));QVERIFY(templatedDocument.contains("Texte générique après"));QVERIFY(templatedDocument.contains("3 Requirements"));
  QVERIFY(templatedDocument.contains("Sommaire existant"));QVERIFY(templatedDocument.contains(" TOC "));
  QVERIFY(templatedDocument.contains("Specification"));QVERIFY(!templatedDocument.contains("Ancien titre"));
  QVERIFY(templatedDocument.contains("w:tblStyle w:val=\"ReqTable\""));QVERIFY(templatedDocument.contains("REQ-1"));QVERIFY(templatedDocument.contains("Dépend de REQ-3"));QVERIFY(templatedDocument.contains("Interface critique"));
  QVERIFY(templatedDocument.contains("Dérive de REQ-2 — Child"));
  QVERIFY(templatedDocument.contains("Choix d&apos;architecture"));
  QVERIFY(!templatedDocument.contains("Dérive de REQ-1"));
  QVERIFY(!templatedDocument.contains("Texte obsolète distinctif"));
  QVERIFY(templatedDocument.contains("w:pStyle w:val=\"Custom4\""));QVERIFY(templatedDocument.contains("w:pStyle w:val=\"Custom5\""));QVERIFY(!templatedDocument.contains("GESIER_CHAPTER_LEVEL"));QVERIFY(!templatedDocument.contains("GESIER_REQUIREMENT_TEMPLATE"));
  QVERIFY(templatedHeader.contains("SPEC-A"));QVERIFY(templatedHeader.contains(">B<"));QVERIFY(!templatedHeader.contains("GESIER_REFERENCE"));QVERIFY(!templatedHeader.contains("GESIER_METADATA"));
  QVERIFY(templatedFooter.contains("Équipe système"));QVERIFY(templatedFooter.contains("Indice : B"));QVERIFY(!templatedFooter.contains("GESIER_METADATA"));
  const QString settings=QString::fromUtf8(templatedZip.fileData("word/settings.xml"));QVERIFY(settings.contains("w:updateFields w:val=\"true\""));
  const QString coreProperties=QString::fromUtf8(templatedZip.fileData("docProps/core.xml"));QVERIFY(coreProperties.contains("<dc:title>Specification</dc:title>"));
  DocxExportRequest withObsolete = templated;
  withObsolete.includeObsoleteRequirements = true;
  withObsolete.outputPath = directory.filePath("templated-obsolete.docx");
  const auto obsoleteResult = exporter.exportDocument(withObsolete);
  QVERIFY2(obsoleteResult.success, qPrintable(obsoleteResult.message));
  QZipReader obsoleteZip(withObsolete.outputPath);
  const QString obsoleteDocument =
      QString::fromUtf8(obsoleteZip.fileData("word/document.xml"));
  QVERIFY(obsoleteDocument.contains("Texte obsolète distinctif"));
  QVERIFY(obsoleteDocument.contains("<w:color w:val=\"808080\"/>"));
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(2,%1,1,'Autre')").arg(root)));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(2,'SPEC-B',1)"));
  QVERIFY(documents.placeRequirement(2,-1,1).success);
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(3,%1,1,'Ordre')").arg(root)));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(3,'SPEC-C',1)"));
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
      "CREATE TABLE REQ_IMAGE(ID INTEGER PRIMARY KEY,REQ_ID INTEGER NOT NULL,"
      "LEGEND TEXT,IMAGE BLOB NOT NULL)",
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
      "UPDATE REQUIREMENT SET DESCRIPTION='<p><b>Description legacy</b></p>' "
      "WHERE ID=1",
      "INSERT INTO REQ_IMAGE(ID,REQ_ID,LEGEND,IMAGE) "
      "VALUES(7,1,'Schéma historique',x'89504E47')",
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
  QVERIFY(query.exec("SELECT DESCRIPTION FROM REQUIREMENT WHERE ID=1"));
  QVERIFY(query.next());
  QVERIFY(query.value(0).toString().contains("**Description legacy**"));
  QVERIFY(query.value(0).toString().contains("gesier-image://legacy-7"));
  QVERIFY(query.exec("SELECT TOKEN,MIME_TYPE,LEGEND,hex(IMAGE) FROM REQ_IMAGE WHERE ID=7"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toString(), QString("legacy-7"));
  QCOMPARE(query.value(1).toString(), QString("image/png"));
  QCOMPARE(query.value(2).toString(), QString("Schéma historique"));
  QCOMPARE(query.value(3).toString(), QString("89504E47"));
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
  QCOMPARE(requirements.suggestCode(cId), QString("SYS-C-R-0000"));
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

void DatabaseMigratorTest::migratesRequirementUniquenessToCode() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  REQ_SQLManager manager;
  QVERIFY(manager.newDB(directory.filePath("requirements.db")).type() ==
          QSqlError::NoError);
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery query(db);
  QVERIFY(query.exec("INSERT INTO PT(ID,NAME,SEGMENT) "
                     "VALUES(1,'Système','SYS')"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
                     "VALUES(1,1,1,'Spécification')"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'REQ-001','Premier titre',1,1)"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(2,'REQ-002','Second titre',1,1)"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) "
                     "VALUES(1,1,1),(2,1,1)"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,"
                     "TARGET_REQ_ID,TYPE_ID) VALUES(1,2,1)"));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_NODE(DOC_ID,NODE_TYPE,POSITION,"
                     "REQ_ID) VALUES(1,'REQUIREMENT',1,1)"));

  QVERIFY(query.exec("SELECT sql FROM sqlite_master WHERE type='table' AND "
                     "name='REQUIREMENT'"));
  QVERIFY(query.next());
  QString version15Schema = query.value(0).toString();
  version15Schema.replace("CREATE TABLE REQUIREMENT",
                          "CREATE TABLE REQUIREMENT_V15");
  version15Schema.replace("TITLE STRING NOT NULL",
                          "TITLE STRING NOT NULL UNIQUE");
  version15Schema.replace("TITLE TEXT NOT NULL",
                          "TITLE TEXT NOT NULL UNIQUE");
  query.finish();
  QVERIFY(query.exec("PRAGMA foreign_keys=OFF"));
  QVERIFY2(query.exec(version15Schema), qPrintable(query.lastError().text()));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT_V15 SELECT * FROM REQUIREMENT"));
  QVERIFY(query.exec("DROP TABLE REQUIREMENT"));
  QVERIFY(query.exec("ALTER TABLE REQUIREMENT_V15 RENAME TO REQUIREMENT"));
  QVERIFY(query.exec("PRAGMA user_version=15"));
  QVERIFY(query.exec("PRAGMA foreign_keys=ON"));

  QString error;
  QVERIFY2(DatabaseMigrator::migrate(db, &error), qPrintable(error));
  QVERIFY(query.exec("UPDATE REQUIREMENT SET TITLE='Titre commun'"));
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT WHERE TITLE='Titre "
                     "commun'"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 2);
  QVERIFY(!query.exec("INSERT INTO REQUIREMENT(CODE,TITLE,TYPE,STATUS) "
                      "VALUES('req-001','Autre titre',1,1)"));
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_PT"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 2);
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_RELATION"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 1);
  QVERIFY(query.exec("SELECT COUNT(*) FROM DOCUMENT_NODE WHERE REQ_ID=1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 1);
  QVERIFY2(DatabaseMigrator::migrate(db, &error), qPrintable(error));
  query = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

void DatabaseMigratorTest::rejectsCaseInsensitiveCodeCollisions() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "collision-test");
  db.setDatabaseName(directory.filePath("collision.db"));
  QVERIFY(db.open());
  QSqlQuery query(db);
  QVERIFY(query.exec("CREATE TABLE REQUIREMENT(ID INTEGER PRIMARY KEY,CODE "
                     "TEXT,TITLE TEXT NOT NULL UNIQUE)"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT VALUES(1,'REQ-001','Premier')"));
  QVERIFY(query.exec("INSERT INTO REQUIREMENT VALUES(2,'req-001','Second')"));
  QVERIFY(query.exec("PRAGMA user_version=15"));
  QString error;
  QVERIFY(!DatabaseMigrator::migrate(db, &error));
  QVERIFY(error.contains("REQ-001"));
  QVERIFY(error.contains("req-001"));
  QVERIFY(query.exec("PRAGMA user_version"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 15);
  QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 2);
  query = QSqlQuery();
  db.close();
  db = QSqlDatabase();
  QSqlDatabase::removeDatabase("collision-test");
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
  QVERIFY(query.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'SYS-001','Requirement',1,1)"));
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
  QVERIFY(query.exec("SELECT METHOD_ID,VERIF_LEVEL,PROCEDURE_REF,VERDICT "
                     "FROM REQUIREMENT_VERIFICATION WHERE REQ_ID=1 "
                     "ORDER BY POSITION LIMIT 1"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 1);
  QCOMPARE(query.value(1).toString(), QString("System"));
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
  QVERIFY(query.exec("INSERT INTO CONFIGURATION(CODE,LABEL,POSITION) "
                     "VALUES('FM','Flight Model',10)"));
  QVERIFY(query.exec("SELECT ID FROM CONFIGURATION WHERE CODE='FM'"));
  QVERIFY(query.next()); const int fmConfigurationId=query.value(0).toInt();
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT_APPLICABILITY(REQ_ID,CONFIG_ID,PT_ID) "
      "VALUES(1,"+QString::number(fmConfigurationId)+",1)"));
  RequirementFilter applicable;
  applicable.includeObsolete = true;
  applicable.configurationIds = {fmConfigurationId};
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
  QVERIFY(query.exec("SELECT COUNT(*) FROM CONFIGURATION"));
  QVERIFY(query.next());
  QCOMPARE(query.value(0).toInt(), 0);
  query = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

void DatabaseMigratorTest::managesRequirementReferenceData() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  REQ_SQLManager manager;
  const QSqlError error = manager.newDB(directory.filePath("reference-data.db"));
  QVERIFY2(error.type() == QSqlError::NoError, qPrintable(error.text()));
  ReferenceDataService service(manager.currentConnection());

  auto method = service.addVerificationMethod("  Inspection  ");
  QVERIFY2(method.success, qPrintable(method.message));
  QVERIFY(!service.addVerificationMethod("inspection").success);
  QVERIFY(!service.addVerificationMethod("  ").success);
  QVERIFY(service.updateVerificationMethod(method.id, "Inspection visuelle").success);

  auto type = service.addRequirementType("  PERF-CUSTOM  ", " Performance spéciale ");
  QVERIFY2(type.success, qPrintable(type.message));
  QVERIFY(!service.addRequirementType("perf-custom", "Autre").success);
  QVERIFY(!service.addRequirementType("OTHER", "performance SPÉCIALE").success);
  QVERIFY(!service.addRequirementType({}, " ").success);
  QVERIFY(service.updateRequirementType(type.id, {}, "Performance personnalisée").success);

  auto freeMethod = service.addVerificationMethod("Méthode temporaire");
  auto freeType = service.addRequirementType({}, "Type temporaire");
  QVERIFY(freeMethod.success);
  QVERIFY(freeType.success);
  QVERIFY(service.deleteVerificationMethod(freeMethod.id).success);
  QVERIFY(service.deleteRequirementType(freeType.id).success);

  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "SYS").success);
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection());
  QSqlQuery query(db);
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));
  QVERIFY(query.next());
  const int ptId = query.value(0).toInt();
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,1,'Référentiels')").arg(ptId)));
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'REF-1','Référence',%1,1)").arg(type.id)));
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT_VERIFICATION(REQ_ID,METHOD_ID,POSITION) VALUES(1,%1,0)").arg(method.id)));
  QVERIFY(!service.deleteRequirementType(type.id).success);
  QVERIFY(!service.deleteVerificationMethod(method.id).success);

  auto planMethod = service.addVerificationMethod("Méthode du plan");
  QVERIFY(planMethod.success);
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT_VERIFICATION(REQ_ID,METHOD_ID,POSITION) VALUES(1,%1,0)").arg(planMethod.id)));
  QVERIFY(!service.deleteVerificationMethod(planMethod.id).success);

  const auto methods = service.verificationMethods();
  QVERIFY(std::any_of(methods.cbegin(), methods.cend(), [method](const ReferenceDataEntry &entry) {
    return entry.id == method.id && entry.label == "Inspection visuelle";
  }));
  const auto types = service.requirementTypes();
  QVERIFY(std::any_of(types.cbegin(), types.cend(), [type](const ReferenceDataEntry &entry) {
    return entry.id == type.id && entry.code.isEmpty() && entry.label == "Performance personnalisée";
  }));
  query = QSqlQuery();
  db = QSqlDatabase();
  manager.close();
}

void DatabaseMigratorTest::roundTripsStructuredDocxCollections() {
  QCOMPARE(DocxImportService::normalizedRelationTypeCode("DECOMPOSE"), QString("DECOMPOSE"));
  QCOMPARE(DocxImportService::normalizedRelationTypeCode("DERIVES_FROM"), QString("DERIVES_FROM"));
  QCOMPARE(DocxImportService::normalizedRelationTypeCode("DEPENDS_ON"), QString("DEPENDS_ON"));
  QCOMPARE(DocxImportService::normalizedRelationTypeCode("Enfant de"), QString("DECOMPOSE"));
  QCOMPARE(DocxImportService::normalizedRelationTypeCode("Derives from"), QString("DERIVES_FROM"));
  QVERIFY(DocxImportService::normalizedRelationTypeCode("AUTRE").isEmpty());
  ApplicationLanguage::setCurrent("fr");
  QTemporaryDir directory; QVERIFY(directory.isValid());
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("structured-docx.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  QSqlDatabase db = QSqlDatabase::database(manager.currentConnection()); QSqlQuery q(db);
  QVERIFY(q.exec("INSERT INTO PT(ID,PARENT,NAME,SEGMENT,POSITION) VALUES(101,NULL,'Système','SYS',0),(102,101,'Puissance','PWR',0),(103,102,'Contrôle','CTRL',0)"));
  QVERIFY(q.exec("INSERT INTO CONFIGURATION(CODE,LABEL,POSITION) "
                 "VALUES('FM','Flight Model',10)"));
  QVERIFY(q.exec("SELECT ID FROM CONFIGURATION WHERE CODE='FM'"));QVERIFY(q.next());const int fmId=q.value(0).toInt();
  QVERIFY(q.exec("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,101,1,'Structurée')"));
  QVERIFY(q.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SPEC-STRUCT',1)"));
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,DESCRIPTION,TYPE,STATUS) VALUES(2,'PARENT-1','Parent','Parent',1,1),(3,'ORIGIN-1','Origine','Origine',1,1),(4,'DEPEND-1','Dépendance','Dépendance',1,1)"));
  RequirementRecord record; record.code="REQ-STRUCT"; record.title="Exigence structurée"; record.description="Description"; record.primaryPtId=101; record.ptIds={101}; record.configurationIds={fmId}; record.typeId=1; record.statusId=1;
  RequirementVerification first; first.methodId=1; first.levelPtId=103; first.procedure="PROC-A"; first.redmine="RM-1"; first.means="Banc"; first.verdict="C"; first.comment="Premier";
  RequirementVerification second; second.methodId=2; second.level="Niveau historique"; second.procedure="PROC-B"; second.redmine="RM-2"; second.means="Simulateur"; second.verdict="PC"; second.comment="Second";
  record.verifications={first,second};
  const auto saved=RequirementService(manager.currentConnection()).save(record); QVERIFY2(saved.success,qPrintable(saved.message));
  QVERIFY(DocumentService(manager.currentConnection()).placeRequirement(1,-1,saved.id).success);
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID,COMMENT) VALUES(2,%1,1,'Parentage'),(3,%1,2,'Origine métier'),(%1,4,3,'Prérequis')").arg(saved.id)));
  const QString model=writeStructuredDocxTemplate(directory.filePath("structured-model.docx"));
  DocxExportService exporter(manager.currentConnection());
  const auto validation=exporter.validateTemplate(model,1); QVERIFY2(validation.valid,qPrintable(validation.errors.join('\n')));
  DocxExportRequest translated; translated.documentId=1; translated.templatePath=model; translated.outputPath=directory.filePath("translated.docx");
  QVERIFY2(exporter.exportDocument(translated).success,"L'export structuré traduit doit réussir");
  QZipReader translatedZip(translated.outputPath); const QString translatedXml=QString::fromUtf8(translatedZip.fileData("word/document.xml"));
  QVERIFY(translatedXml.contains("SYS-PWR-CTRL")); QVERIFY(translatedXml.contains("Niveau historique"));
  QVERIFY(translatedXml.contains(">FM<")); QVERIFY(!translatedXml.contains("Applicabilité</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>SYS-PWR-CTRL"));
  QVERIFY(translatedXml.contains("Enfant de")); QVERIFY(translatedXml.contains("Dérive de")); QVERIFY(translatedXml.contains("Dépend de"));
  QVERIFY(!translatedXml.contains(">IN<")); QVERIFY(!translatedXml.contains(">OUT<")); QVERIFY(!translatedXml.contains("RELATION_DIRECTION"));
  QVERIFY(!translatedXml.contains("GESIER_VERIFICATION_TEMPLATE")); QVERIFY(!translatedXml.contains("GESIER_RELATION_TEMPLATE"));
  auto preview=DocxImportService(manager.currentConnection()).preview(translated.outputPath,model);
  QVERIFY2(preview.valid(),qPrintable(preview.errors.join('\n'))); QCOMPARE(preview.requirements.size(),1);
  const auto imported=preview.requirements.first(); QCOMPARE(imported.record.verifications.size(),2); QCOMPARE(imported.verificationMethods,QStringList({"Analysis","Test"}));
  QCOMPARE(imported.record.verifications[0].levelPtId,103); QCOMPARE(imported.record.verifications[1].level,QString("Niveau historique"));
  QVERIFY(std::any_of(imported.detailedDiagnostics.cbegin(), imported.detailedDiagnostics.cend(), [](const auto &detail) { return detail.field=="verification.level" && detail.blocking; }));
  QCOMPARE(imported.configurationCodes,QStringList({"FM"})); QCOMPARE(imported.relations.size(),3);
  QCOMPARE(imported.relations[0].direction,QString("IN")); QCOMPARE(imported.relations[1].direction,QString("IN")); QCOMPARE(imported.relations[2].direction,QString("OUT"));
  auto duplicateDraft=preview; duplicateDraft.requirements[0].selected=true; duplicateDraft.requirements[0].verificationMethods={"Analysis","Analysis"};
  for(auto &verification:duplicateDraft.requirements[0].record.verifications){verification.level.clear();verification.levelPtId=-1;}
  QVERIFY(!DocxImportService(manager.currentConnection()).validatePreview(&duplicateDraft));
  QVERIFY(std::any_of(duplicateDraft.requirements[0].detailedDiagnostics.cbegin(), duplicateDraft.requirements[0].detailedDiagnostics.cend(), [](const auto &detail) { return detail.field=="verification.duplicate"; }));
  auto distinctLevels=duplicateDraft; distinctLevels.requirements[0].record.verifications[0].levelPtId=102; distinctLevels.requirements[0].record.verifications[1].levelPtId=103;
  QVERIFY(DocxImportService(manager.currentConnection()).validatePreview(&distinctLevels));
  preview.requirements[0].record.verifications[1].level.clear(); preview.requirements[0].record.verifications[1].levelPtId=102;
  QVERIFY(DocxImportService(manager.currentConnection()).validatePreview(&preview));
  DocxImportOptions importOptions; importOptions.documentId=1; importOptions.primaryPtId=101;
  const auto importedResult=DocxImportService(manager.currentConnection()).importPreview(preview,importOptions); QVERIFY2(importedResult.success,qPrintable(importedResult.message));
  const auto reloaded=RequirementService(manager.currentConnection()).get(saved.id); QCOMPARE(reloaded.verifications.size(),2); QCOMPARE(reloaded.verifications[0].levelPtId,103); QCOMPARE(reloaded.verifications[1].levelPtId,102);
  QVERIFY(q.exec(QString("SELECT COUNT(*) FROM REQUIREMENT_RELATION WHERE (SOURCE_REQ_ID=2 AND TARGET_REQ_ID=%1 AND TYPE_ID=1) OR (SOURCE_REQ_ID=3 AND TARGET_REQ_ID=%1 AND TYPE_ID=2) OR (SOURCE_REQ_ID=%1 AND TARGET_REQ_ID=4 AND TYPE_ID=3)").arg(saved.id))); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(),3);
  QVERIFY(q.exec(QString("SELECT COMMENT FROM REQUIREMENT_RELATION WHERE SOURCE_REQ_ID=3 AND TARGET_REQ_ID=%1 AND TYPE_ID=2").arg(saved.id))); QVERIFY(q.next()); QCOMPARE(q.value(0).toString(),QString("Origine métier"));
  DocxExportRequest technical=translated; technical.outputPath=directory.filePath("technical.docx"); technical.relationPresentation=DocxExportRequest::RelationPresentation::TechnicalCodes;
  QVERIFY(exporter.exportDocument(technical).success); QZipReader technicalZip(technical.outputPath); const QString technicalXml=QString::fromUtf8(technicalZip.fileData("word/document.xml"));
  QVERIFY(technicalXml.contains("DECOMPOSE")); QVERIFY(technicalXml.contains("DERIVES_FROM")); QVERIFY(technicalXml.contains("DEPENDS_ON")); QVERIFY(!technicalXml.contains(">IN<")); QVERIFY(!technicalXml.contains(">OUT<"));
  ApplicationLanguage::setCurrent("en");
  QCOMPARE(ApplicationLanguage::current(),QString("en"));
  QCOMPARE(ApplicationLanguage::relationLabel("DECOMPOSE"),QString("Child of"));
  DocxExportRequest english=translated; english.outputPath=directory.filePath("english.docx");
  QVERIFY(exporter.exportDocument(english).success); QZipReader englishZip(english.outputPath); const QString englishXml=QString::fromUtf8(englishZip.fileData("word/document.xml"));
  QVERIFY(englishXml.contains("Child of")); QVERIFY(englishXml.contains("Derives from")); QVERIFY(englishXml.contains("Depends on"));
  const auto englishPreview=DocxImportService(manager.currentConnection()).preview(english.outputPath,model); QVERIFY2(englishPreview.valid(),qPrintable(englishPreview.errors.join('\n'))); QCOMPARE(englishPreview.requirements.first().relations.size(),3);
  ApplicationLanguage::setCurrent("fr");
  manager.close();
}

QTEST_GUILESS_MAIN(DatabaseMigratorTest)
#include "database_migrator_test.moc"
