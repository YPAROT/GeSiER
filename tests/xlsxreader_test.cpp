#include <QTemporaryDir>
#include <QtCore/private/qzipwriter_p.h>
#include <QtTest>

#include "xlsxreader.h"

class XlsxReaderTest : public QObject {
  Q_OBJECT
private slots:
  void readsSheetNamesSharedStringsAndInlineValues();
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
  zip.addFile("xl/worksheets/sheet1.xml",
              "<worksheet><sheetData><row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c><c r=\"B1\" t=\"inlineStr\"><is><t>Titre</t></is></c></row><row r=\"2\"><c r=\"A2\" t=\"s\"><v>1</v></c><c r=\"B2\" t=\"inlineStr\"><is><t>Exigence</t></is></c></row></sheetData><autoFilter ref=\"A1:B2\"/></worksheet>");
  zip.close();
  QString error;
  const QList<XlsxSheet> sheets = XlsxReader::read(path, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QCOMPARE(sheets.size(), 1);
  QCOMPARE(sheets.first().name, QString("High level"));
  QCOMPARE(sheets.first().rows.value(1).value(0), QString("REQ-001"));
  QCOMPARE(sheets.first().rows.value(1).value(1), QString("Exigence"));
}

QTEST_GUILESS_MAIN(XlsxReaderTest)
#include "xlsxreader_test.moc"
