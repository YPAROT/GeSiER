#include <QTemporaryDir>
#include <QFile>
#include <QtCore/private/qzipwriter_p.h>
#include <QtTest>

#include "xlsxreader.h"
#include "tabularservice.h"

class XlsxReaderTest : public QObject {
  Q_OBJECT
private slots:
  void readsSheetNamesSharedStringsAndInlineValues();
  void readsCsvDialectsAndQuotedNewlines();
  void writesMultiSheetWorkbookAndProfiles();
};

void XlsxReaderTest::readsSheetNamesSharedStringsAndInlineValues() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath("requirements.xlsx");
  QZipWriter zip(path);
  zip.addFile("xl/workbook.xml",
              "<workbook xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets><sheet name=\"High level\" r:id=\"rId1\"/></sheets></workbook>");
  zip.addFile("xl/_rels/workbook.xml.rels",
              "<Relationships><Relationship Id=\"rId1\" Target=\"/xl/worksheets/sheet1.xml\"/></Relationships>");
  zip.addFile("xl/sharedStrings.xml",
              "<sst><si><t>Code</t></si><si><t>REQ-001</t></si></sst>");
  zip.addFile("xl/styles.xml",
              "<styleSheet><cellXfs count=\"2\"><xf numFmtId=\"0\"/><xf numFmtId=\"14\"/></cellXfs></styleSheet>");
  zip.addFile("xl/worksheets/sheet1.xml",
              "<worksheet><sheetData><row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c><c r=\"B1\" t=\"inlineStr\"><is><t>Titre</t></is></c></row><row r=\"2\"><c r=\"A2\" t=\"s\"><v>1</v></c><c r=\"B2\" t=\"inlineStr\"><is><t>Exigence</t></is></c><c r=\"C2\" t=\"b\"><v>1</v></c><c r=\"D2\" s=\"1\"><v>45292</v></c></row></sheetData><autoFilter ref=\"A1:D2\"/></worksheet>");
  zip.close();
  QString error;
  const QList<XlsxSheet> sheets = XlsxReader::read(path, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(sheets.size(), 1);
  QCOMPARE(sheets.first().name, QString("High level"));
  QCOMPARE(sheets.first().rows.value(1).value(0), QString("REQ-001"));
  QCOMPARE(sheets.first().rows.value(1).value(1), QString("Exigence"));
  QCOMPARE(sheets.first().rows.value(1).value(2), QString("VRAI"));
  QCOMPARE(sheets.first().rows.value(1).value(3), QString("2024-01-01"));
}

void XlsxReaderTest::readsCsvDialectsAndQuotedNewlines() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath("requirements.csv");
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(QByteArray::fromHex("efbbbf") +
             "Code;Titre;Description\r\nREQ-1;Titre;\"Une ligne\navec retour\"\r\nREQ-2;;Vide\r\n");
  file.close();
  QString error;
  const auto workbook = TabularService::read(path, {}, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(workbook.sheets.size(), 1);
  QCOMPARE(workbook.sheets.first().rows.size(), 3);
  QCOMPARE(workbook.sheets.first().rows[1][2], QString("Une ligne\navec retour"));
  QCOMPARE(workbook.sheets.first().rows[2][1], QString());
  QCOMPARE(workbook.sheets.first().rows[0][0], QString("Code"));

  const QString legacyPath = directory.filePath("legacy.csv");
  QFile legacy(legacyPath);
  QVERIFY(legacy.open(QIODevice::WriteOnly));
  legacy.write(QByteArray("Code;Titre\r\nREQ-3;R") + char(0xe9) + "sum" + char(0xe9));
  legacy.close();
  TabularReadOptions options;
  options.encoding = TabularEncoding::Windows1252;
  const auto legacyWorkbook = TabularService::read(legacyPath, options, &error);
  QCOMPARE(legacyWorkbook.sheets.first().rows[1][1], QString::fromUtf8("Résumé"));

  const QString largePath = directory.filePath("large.csv");
  QFile large(largePath);
  QVERIFY(large.open(QIODevice::WriteOnly));
  QByteArray payload("Code;Titre\r\n");
  for (int row = 0; row < 10000; ++row)
    payload += "REQ-" + QByteArray::number(row) + ";Titre " + QByteArray::number(row) + "\r\n";
  QCOMPARE(large.write(payload), qint64(payload.size()));
  large.close();
  const auto largeWorkbook = TabularService::read(largePath, {}, &error);
  QCOMPARE(largeWorkbook.sheets.first().rows.size(), 10001);
}

void XlsxReaderTest::writesMultiSheetWorkbookAndProfiles() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath("matrix.xlsx");
  QString error;
  QVERIFY2(TabularService::writeXlsx(
               path, {{"Synthèse", {{"Indicateur", "Valeur"}}},
                      {"Détails", {{"Code", "Titre"}, {"REQ-1", "Test"}}},
                      {"Anomalies", {{"Code", "Anomalie"}}}}, &error),
           qPrintable(error));
  const auto workbook = TabularService::read(path, {}, &error);
  QCOMPARE(workbook.sheets.size(), 3);
  QCOMPARE(workbook.sheets[1].rows[1][0], QString("REQ-1"));

  TabularProfile profile;
  profile.name = "default";
  profile.sheetName = "Détails";
  profile.headerRow = 2;
  profile.ignoredRows = 3;
  profile.separator = ';';
  profile.mappings << TabularColumnMapping{"code", "Identifiant", "Code", 4,
                                           TabularTransform::Uppercase};
  TabularProfile restored;
  QVERIFY(TabularService::deserializeProfile(
      TabularService::serializeProfile(profile), &restored, &error));
  QCOMPARE(restored.sheetName, profile.sheetName);
  QCOMPARE(restored.ignoredRows, 3);
  QCOMPARE(restored.mappings.first().sourceColumn, 4);
  QCOMPARE(TabularService::transform("  req-1 ", restored.mappings.first().transform),
           QString("REQ-1"));
}

QTEST_GUILESS_MAIN(XlsxReaderTest)
#include "xlsxreader_test.moc"
