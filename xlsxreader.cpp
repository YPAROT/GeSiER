#include "xlsxreader.h"

#include <QMap>
#include <QDir>
#include <QXmlStreamReader>
#include <QtCore/private/qzipreader_p.h>

namespace {
int columnNumber(const QString &reference) {
  int result = 0;
  for (QChar character : reference) {
    if (!character.isLetter())
      break;
    result = result * 26 + character.toUpper().unicode() - 'A' + 1;
  }
  return result - 1;
}

QStringList sharedStrings(const QByteArray &xml) {
  QStringList result;
  QXmlStreamReader reader(xml);
  QString current;
  bool inItem = false;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement() && reader.name() == u"si") {
      current.clear();
      inItem = true;
    } else if (inItem && reader.isStartElement() && reader.name() == u"t") {
      current += reader.readElementText();
    } else if (reader.isEndElement() && reader.name() == u"si") {
      result << current;
      inItem = false;
    }
  }
  return result;
}

QVector<QStringList> sheetRows(const QByteArray &xml,
                               const QStringList &shared) {
  QVector<QStringList> result;
  QXmlStreamReader reader(xml);
  QStringList row;
  int column = 0;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement() && reader.name() == u"row") {
      row.clear();
    } else if (reader.isStartElement() && reader.name() == u"c") {
      column = columnNumber(reader.attributes().value("r").toString());
      const QString type = reader.attributes().value("t").toString();
      QString value;
      while (!(reader.isEndElement() && reader.name() == u"c") &&
             !reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() &&
            (reader.name() == u"v" || reader.name() == u"t"))
          value += reader.readElementText();
      }
      if (type == "s") {
        bool ok = false;
        const int index = value.toInt(&ok);
        value = ok && index >= 0 && index < shared.size() ? shared[index]
                                                          : QString();
      }
      while (row.size() <= column)
        row << QString();
      row[column] = value;
    } else if (reader.isEndElement() && reader.name() == u"row") {
      result << row;
    }
  }
  return result;
}
} // namespace

QList<XlsxSheet> XlsxReader::read(const QString &fileName, QString *error) {
  QZipReader zip(fileName);
  if (!zip.exists() || !zip.isReadable()) {
    if (error)
      *error = "Le fichier XLSX ne peut pas être ouvert.";
    return {};
  }
  const QStringList shared =
      sharedStrings(zip.fileData("xl/sharedStrings.xml"));
  QMap<QString, QString> targets;
  QXmlStreamReader relationships(
      zip.fileData("xl/_rels/workbook.xml.rels"));
  while (!relationships.atEnd()) {
    relationships.readNext();
    if (relationships.isStartElement() &&
        relationships.name() == u"Relationship") {
      QString target = relationships.attributes().value("Target").toString();
      target.replace('\\', '/');
      while (target.startsWith('/'))
        target.remove(0, 1);
      if (!target.startsWith("xl/"))
        target = QDir::cleanPath("xl/" + target);
      targets[relationships.attributes().value("Id").toString()] = target;
    }
  }
  QList<XlsxSheet> result;
  QXmlStreamReader workbook(zip.fileData("xl/workbook.xml"));
  while (!workbook.atEnd()) {
    workbook.readNext();
    if (!workbook.isStartElement() || workbook.name() != u"sheet")
      continue;
    const QString name = workbook.attributes().value("name").toString();
    QString relationship = workbook.attributes().value(
        "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
        "id").toString();
    if (relationship.isEmpty()) {
      for (const QXmlStreamAttribute &attribute : workbook.attributes())
        if (attribute.name() == u"id") {
          relationship = attribute.value().toString();
          break;
        }
    }
    const QString target = targets.value(relationship);
    if (!target.isEmpty())
      result << XlsxSheet{name, sheetRows(zip.fileData(target), shared)};
  }
  if (result.isEmpty() && error)
    *error = "Aucun onglet lisible n'a été trouvé dans le classeur.";
  return result;
}
