#include "tabularservice.h"

#include "csvutility.h"
#include "xlsxreader.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QXmlStreamWriter>
#include <QtCore/private/qzipwriter_p.h>

namespace {
QString columnName(int column) {
  QString result;
  for (++column; column; column = (column - 1) / 26)
    result.prepend(QChar('A' + (column - 1) % 26));
  return result;
}

QString safeSheetName(QString name, int index) {
  name.replace(QRegularExpression("[\\\\/:?*\\[\\]]"), " ");
  name = name.simplified().left(31);
  return name.isEmpty() ? QString("Feuille %1").arg(index + 1) : name;
}

QByteArray sheetXml(const QList<QStringList> &rows) {
  QByteArray data;
  QXmlStreamWriter x(&data);
  x.writeStartDocument();
  x.writeStartElement("worksheet");
  x.writeDefaultNamespace(
      "http://schemas.openxmlformats.org/spreadsheetml/2006/main");
  x.writeStartElement("sheetData");
  for (int row = 0; row < rows.size(); ++row) {
    x.writeStartElement("row");
    x.writeAttribute("r", QString::number(row + 1));
    for (int column = 0; column < rows[row].size(); ++column) {
      x.writeStartElement("c");
      x.writeAttribute("r", columnName(column) + QString::number(row + 1));
      x.writeAttribute("t", "inlineStr");
      x.writeStartElement("is");
      x.writeTextElement("t", rows[row][column]);
      x.writeEndElement();
      x.writeEndElement();
    }
    x.writeEndElement();
  }
  x.writeEndElement();
  x.writeEndElement();
  x.writeEndDocument();
  return data;
}
} // namespace

TabularWorkbook TabularService::read(const QString &path,
                                     const TabularReadOptions &options,
                                     QString *error) {
  if (error)
    error->clear();
  TabularFormat format = options.format;
  if (format == TabularFormat::Auto)
    format = path.endsWith(".csv", Qt::CaseInsensitive) ? TabularFormat::Csv
                                                        : TabularFormat::Xlsx;
  TabularWorkbook workbook;
  if (format == TabularFormat::Xlsx) {
    const auto sheets = XlsxReader::read(path, error);
    for (const auto &sheet : sheets)
      workbook.sheets << TabularSheet{sheet.name, sheet.rows};
    return workbook;
  }
  CsvReadOptions csv;
  csv.separator = options.separator;
  csv.encoding = options.encoding == TabularEncoding::Windows1252
                     ? CsvEncoding::Windows1252
                     : options.encoding == TabularEncoding::Utf8
                           ? CsvEncoding::Utf8
                           : CsvEncoding::Auto;
  const auto rows = CsvUtility::read(path, csv, error);
  if (!rows.isEmpty() || !error || error->isEmpty())
    workbook.sheets << TabularSheet{QFileInfo(path).completeBaseName(), rows};
  return workbook;
}

bool TabularService::writeXlsx(const QString &path,
                               const QList<TabularSheet> &sheets,
                               QString *error) {
  if (sheets.isEmpty()) {
    if (error)
      *error = "Le classeur ne contient aucune feuille.";
    return false;
  }
  QZipWriter zip(path);
  if (zip.status() != QZipWriter::NoError) {
    if (error)
      *error = "Impossible de créer le fichier XLSX.";
    return false;
  }
  QByteArray types = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>";
  for (int i = 0; i < sheets.size(); ++i)
    types += "<Override PartName=\"/xl/worksheets/sheet" +
             QByteArray::number(i + 1) +
             ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
  types += "</Types>";
  zip.addFile("[Content_Types].xml", types);
  zip.addFile("_rels/.rels", "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>");
  QByteArray workbook = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>";
  QByteArray rels = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
  for (int i = 0; i < sheets.size(); ++i) {
    const QString name = safeSheetName(sheets[i].name, i);
    workbook += "<sheet name=\"" + name.toHtmlEscaped().toUtf8() +
                "\" sheetId=\"" + QByteArray::number(i + 1) +
                "\" r:id=\"rId" + QByteArray::number(i + 1) + "\"/>";
    rels += "<Relationship Id=\"rId" + QByteArray::number(i + 1) +
            "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet" +
            QByteArray::number(i + 1) + ".xml\"/>";
    zip.addFile("xl/worksheets/sheet" + QByteArray::number(i + 1) + ".xml",
                sheetXml(sheets[i].rows));
  }
  workbook += "</sheets></workbook>";
  rels += "</Relationships>";
  zip.addFile("xl/workbook.xml", workbook);
  zip.addFile("xl/_rels/workbook.xml.rels", rels);
  zip.close();
  if (zip.status() != QZipWriter::NoError || !QFileInfo(path).exists() ||
      QFileInfo(path).size() == 0) {
    if (error)
      *error = "Échec de finalisation du fichier XLSX.";
    return false;
  }
  return true;
}

bool TabularService::writeCsv(const QString &path, const TabularSheet &sheet,
                              QChar separator, QString *error) {
  return CsvUtility::write(path, sheet.rows, separator, error);
}

QString TabularService::transform(QString value,
                                  TabularTransform transformation) {
  if (transformation == TabularTransform::Trim)
    return value.trimmed();
  if (transformation == TabularTransform::Uppercase)
    return value.trimmed().toUpper();
  if (transformation == TabularTransform::Lowercase)
    return value.trimmed().toLower();
  return value;
}

QByteArray TabularService::serializeProfile(const TabularProfile &profile) {
  QJsonObject root{{"name", profile.name},
                   {"format", int(profile.format)},
                   {"sheet", profile.sheetName},
                   {"headerRow", profile.headerRow},
                   {"ignoredRows", profile.ignoredRows},
                   {"separator", QString(profile.separator)},
                   {"encoding", int(profile.encoding)}};
  QJsonArray mappings;
  for (const auto &mapping : profile.mappings)
    mappings.append(QJsonObject{{"target", mapping.target},
                                {"sourceHeader", mapping.sourceHeader},
                                {"outputHeader", mapping.outputHeader},
                                {"sourceColumn", mapping.sourceColumn},
                                {"transform", int(mapping.transform)}});
  root["mappings"] = mappings;
  return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool TabularService::deserializeProfile(const QByteArray &data,
                                        TabularProfile *profile,
                                        QString *error) {
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(data, &parseError);
  if (!profile || !document.isObject()) {
    if (error)
      *error = parseError.error == QJsonParseError::NoError
                   ? "Profil tabulaire invalide."
                   : parseError.errorString();
    return false;
  }
  const auto root = document.object();
  profile->name = root["name"].toString();
  profile->format = TabularFormat(root["format"].toInt());
  profile->sheetName = root["sheet"].toString();
  profile->headerRow = qMax(0, root["headerRow"].toInt());
  profile->ignoredRows = qMax(0, root["ignoredRows"].toInt());
  const QString separator = root["separator"].toString();
  profile->separator = separator.isEmpty() ? QChar() : separator.at(0);
  profile->encoding = TabularEncoding(root["encoding"].toInt());
  profile->mappings.clear();
  for (const auto &item : root["mappings"].toArray()) {
    const auto object = item.toObject();
    profile->mappings << TabularColumnMapping{
        object["target"].toString(), object["sourceHeader"].toString(),
        object["outputHeader"].toString(), object["sourceColumn"].toInt(-1),
        TabularTransform(object["transform"].toInt(int(TabularTransform::Trim)))};
  }
  return true;
}

void TabularService::saveProfile(const QString &scope,
                                 const TabularProfile &profile) {
  QSettings().setValue("TabularProfiles/" + scope + "/" + profile.name,
                       serializeProfile(profile));
}

TabularProfile TabularService::loadProfile(const QString &scope,
                                           const QString &name) {
  TabularProfile profile;
  const QString selected = name.isEmpty() ? "default" : name;
  deserializeProfile(QSettings().value("TabularProfiles/" + scope + "/" +
                                       selected).toByteArray(),
                     &profile);
  return profile;
}
