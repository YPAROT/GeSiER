#include "docximportservice.h"

#include "documentservice.h"
#include "producttreeservice.h"
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextDocumentFragment>
#include <private/qzipreader_p.h>

namespace {
const QString ReqBegin = "{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}";
const QString ReqEnd = "{{GESIER_REQUIREMENT_TEMPLATE_END}}";
const QString VerBegin = "{{GESIER_VERIFICATION_TEMPLATE_BEGIN}}";
const QString VerEnd = "{{GESIER_VERIFICATION_TEMPLATE_END}}";
const QString RelBegin = "{{GESIER_RELATION_TEMPLATE_BEGIN}}";
const QString RelEnd = "{{GESIER_RELATION_TEMPLATE_END}}";
const QStringList ReqTags = {
    "REQ_ID", "REQ_CODE", "REQ_TITLE", "REQ_DESCRIPTION", "REQ_TYPE",
    "REQ_STATUS", "REQ_SOURCE", "REQ_PRODUCT_TREES", "REQ_APPLICABILITY",
    "REQ_VERIFICATIONS", "REQ_RELATIONS"};

struct Paragraph { QString xml, text, style; int outline = 0; };
struct WordCell { QString xml, text, html; };
struct WordTable { QString xml; QList<QList<WordCell>> rows; };
struct BodyItem {
  enum Kind { ParagraphItem, TableItem } kind = ParagraphItem;
  Paragraph paragraph;
  WordTable table;
};

QString decode(QString value) {
  return QTextDocumentFragment::fromHtml(value).toPlainText();
}
QString visible(const QString &xml) {
  QString result;
  QRegularExpression expression("<w:t(?:\\s[^>]*)?>([\\s\\S]*?)</w:t>",
                                QRegularExpression::CaseInsensitiveOption);
  auto matches = expression.globalMatch(xml);
  while (matches.hasNext()) result += decode(matches.next().captured(1));
  return result;
}
QString documentXml(const QString &path, QString *error) {
  if (!QFileInfo::exists(path)) { if (error) *error = "Fichier introuvable : " + path; return {}; }
  QZipReader zip(path);
  if (!zip.exists()) { if (error) *error = "Le fichier n'est pas un DOCX valide."; return {}; }
  const QByteArray data = zip.fileData("word/document.xml");
  if (data.isEmpty() && error) *error = "Le DOCX ne contient pas word/document.xml.";
  return QString::fromUtf8(data);
}
QString stylesXml(const QString &path) {
  QZipReader zip(path); return QString::fromUtf8(zip.fileData("word/styles.xml"));
}
QByteArray partData(const QString &path, const QString &part) {
  QZipReader zip(path);
  return zip.exists() ? zip.fileData(part) : QByteArray();
}
int styleOutline(const QString &style, const QString &styles) {
  QRegularExpression direct("<w:outlineLvl[^>]*w:val=\"(\\d+)\"");
  auto match = direct.match(style);
  if (match.hasMatch()) return match.captured(1).toInt() + 1;
  QRegularExpression id("<w:pStyle[^>]*w:val=\"([^\"]+)\"");
  match = id.match(style);
  if (!match.hasMatch()) return 0;
  const QString sid = QRegularExpression::escape(match.captured(1));
  QRegularExpression definition("<w:style[^>]*w:styleId=\"" + sid + "\"[\\s\\S]*?</w:style>");
  auto defined = definition.match(styles);
  if (defined.hasMatch()) {
    auto level = direct.match(defined.captured());
    if (level.hasMatch()) return level.captured(1).toInt() + 1;
  }
  QRegularExpression heading("(?:Heading|Titre)([1-6])", QRegularExpression::CaseInsensitiveOption);
  auto named = heading.match(match.captured(1));
  return named.hasMatch() ? named.captured(1).toInt() : 0;
}
QList<Paragraph> paragraphs(const QString &xml, const QString &styles = {}) {
  QList<Paragraph> result;
  QRegularExpression expression("<w:p(?:\\s[^>]*)?>[\\s\\S]*?</w:p>",
                                QRegularExpression::CaseInsensitiveOption);
  auto matches = expression.globalMatch(xml);
  while (matches.hasNext()) {
    const QString paragraph = matches.next().captured();
    QRegularExpression props("<w:pPr(?:\\s[^>]*)?>[\\s\\S]*?</w:pPr>");
    const QString style = props.match(paragraph).captured();
    result << Paragraph{paragraph, visible(paragraph).trimmed(), style,
                        styleOutline(style, styles)};
  }
  return result;
}
QString normalized(QString value) { return value.simplified().toUpper(); }
QString htmlFromParagraph(const Paragraph &p) {
  QString text;
  QRegularExpression runs("<w:r(?:\\s[^>]*)?>[\\s\\S]*?</w:r>");
  auto iterator = runs.globalMatch(p.xml);
  while (iterator.hasNext()) {
    const QString run = iterator.next().captured();
    QString value = visible(run).toHtmlEscaped();
    if (run.contains(QRegularExpression("<w:b(?:\\s[^>]*)?/?>(?:</w:b>)?"))) value = "<b>" + value + "</b>";
    if (run.contains(QRegularExpression("<w:i(?:\\s[^>]*)?/?>(?:</w:i>)?"))) value = "<i>" + value + "</i>";
    text += value;
  }
  if (p.xml.contains("<w:numPr")) return "<li>" + text + "</li>";
  return "<p>" + text + "</p>";
}
QString cellText(const QString &xml) {
  QStringList values;
  for (const Paragraph &paragraph : paragraphs(xml)) values << paragraph.text;
  while (!values.isEmpty() && values.first().isEmpty()) values.removeFirst();
  while (!values.isEmpty() && values.last().isEmpty()) values.removeLast();
  return values.join('\n').trimmed();
}
QString cellHtml(const QString &xml) {
  QString result;
  bool inList = false;
  for (const Paragraph &paragraph : paragraphs(xml)) {
    const bool list = paragraph.xml.contains("<w:numPr");
    if (list && !inList) { result += "<ul>"; inList = true; }
    if (!list && inList) { result += "</ul>"; inList = false; }
    result += htmlFromParagraph(paragraph);
  }
  if (inList) result += "</ul>";
  return result;
}
WordTable parseTable(const QString &xml) {
  WordTable table; table.xml = xml;
  QRegularExpression rowExpression("<w:tr(?:\\s[^>]*)?>[\\s\\S]*?</w:tr>", QRegularExpression::CaseInsensitiveOption);
  auto rows = rowExpression.globalMatch(xml);
  while (rows.hasNext()) {
    const QString rowXml = rows.next().captured();
    QList<WordCell> row;
    QRegularExpression cellExpression("<w:tc(?:\\s[^>]*)?>[\\s\\S]*?</w:tc>", QRegularExpression::CaseInsensitiveOption);
    auto cells = cellExpression.globalMatch(rowXml);
    while (cells.hasNext()) {
      const QString value = cells.next().captured();
      row << WordCell{value, cellText(value), cellHtml(value)};
    }
    table.rows << row;
  }
  return table;
}
QList<WordTable> tables(const QString &xml) {
  QList<WordTable> result;
  QRegularExpression expression("<w:tbl(?:\\s[^>]*)?>[\\s\\S]*?</w:tbl>", QRegularExpression::CaseInsensitiveOption);
  auto matches = expression.globalMatch(xml);
  while (matches.hasNext()) result << parseTable(matches.next().captured());
  return result;
}
QList<BodyItem> bodyItems(const QString &xml, const QString &styles) {
  QList<BodyItem> result;
  QRegularExpression bodyExpression("<w:body(?:\\s[^>]*)?>([\\s\\S]*?)</w:body>", QRegularExpression::CaseInsensitiveOption);
  const QString body = bodyExpression.match(xml).captured(1);
  QRegularExpression itemExpression("<w:tbl(?:\\s[^>]*)?>[\\s\\S]*?</w:tbl>|<w:p(?:\\s[^>]*)?>[\\s\\S]*?</w:p>", QRegularExpression::CaseInsensitiveOption);
  auto items = itemExpression.globalMatch(body);
  while (items.hasNext()) {
    const QString item = items.next().captured();
    if (item.startsWith("<w:tbl", Qt::CaseInsensitive)) {
      BodyItem value; value.kind = BodyItem::TableItem; value.table = parseTable(item); result << value;
    } else {
      const auto parsed = paragraphs(item, styles);
      if (!parsed.isEmpty()) { BodyItem value; value.paragraph = parsed.first(); result << value; }
    }
  }
  return result;
}
QStringList tagsIn(const QString &text) {
  QStringList result;
  QRegularExpression expression("\\{\\{([A-Z0-9_]+)\\}\\}", QRegularExpression::CaseInsensitiveOption);
  auto matches = expression.globalMatch(text);
  while (matches.hasNext()) result << matches.next().captured(1).toUpper();
  return result;
}
QString valueForTag(const QString &templateText, const QString &sourceText,
                    const QString &tag, bool *matches) {
  const QString marker = "{{" + tag + "}}";
  const int position = templateText.indexOf(marker, 0, Qt::CaseInsensitive);
  if (position < 0) { *matches = false; return {}; }
  const QString prefix = templateText.left(position).trimmed();
  const QString suffix = templateText.mid(position + marker.size()).trimmed();
  QString value = sourceText.trimmed();
  if (!prefix.isEmpty()) {
    if (!value.startsWith(prefix, Qt::CaseInsensitive)) { *matches = false; return {}; }
    value = value.mid(prefix.size()).trimmed();
  }
  if (!suffix.isEmpty()) {
    if (!value.endsWith(suffix, Qt::CaseInsensitive)) { *matches = false; return {}; }
    value.chop(suffix.size()); value = value.trimmed();
  }
  *matches = true;
  return value;
}
bool matchTable(const WordTable &pattern, const WordTable &source,
                QMap<QString, QString> *values,
                QMap<QString, QString> *htmlValues, QString *diagnostic) {
  if (source.rows.size() < pattern.rows.size()) {
    if (diagnostic) *diagnostic = QString("tableau trop court : %1 ligne(s) attendue(s), %2 trouvée(s)").arg(pattern.rows.size()).arg(source.rows.size());
    return false;
  }
  for (int row = 0; row < pattern.rows.size(); ++row) {
    if (source.rows[row].size() < pattern.rows[row].size()) {
      if (diagnostic) *diagnostic = QString("ligne %1 : %2 cellule(s) attendue(s), %3 trouvée(s)").arg(row + 1).arg(pattern.rows[row].size()).arg(source.rows[row].size());
      return false;
    }
    for (int column = 0; column < pattern.rows[row].size(); ++column) {
      const WordCell &expected = pattern.rows[row][column];
      const WordCell &actual = source.rows[row][column];
      const QStringList tags = tagsIn(expected.text);
      if (tags.isEmpty()) {
        // Empty, untagged cells are unspecified zones, not fixed literals.
        if (!expected.text.trimmed().isEmpty() && normalized(expected.text) != normalized(actual.text)) {
          if (diagnostic) *diagnostic = QString("ligne %1, cellule %2 : libellé « %3 » attendu, « %4 » trouvé").arg(row + 1).arg(column + 1).arg(expected.text, actual.text);
          return false;
        }
        continue;
      }
      if (tags.size() > 1) {
        const auto expectedParagraphs = paragraphs(expected.xml);
        const auto actualParagraphs = paragraphs(actual.xml);
        int sourceParagraph = 0, tagIndex = 0;
        for (const Paragraph &expectedParagraph : expectedParagraphs) {
          const QStringList paragraphTags = tagsIn(expectedParagraph.text);
          if (paragraphTags.isEmpty()) {
            if (expectedParagraph.text.isEmpty()) continue;
            if (sourceParagraph >= actualParagraphs.size() || normalized(expectedParagraph.text) != normalized(actualParagraphs[sourceParagraph].text)) {
              if (diagnostic) *diagnostic = QString("ligne %1, cellule %2 : paragraphe fixe « %3 » introuvable").arg(row + 1).arg(column + 1).arg(expectedParagraph.text);
              return false;
            }
            ++sourceParagraph; continue;
          }
          for (const QString &tag : paragraphTags) {
            ++tagIndex;
            if (sourceParagraph >= actualParagraphs.size()) {
              if (diagnostic) *diagnostic = QString("ligne %1, cellule %2 : aucune valeur pour %3").arg(row + 1).arg(column + 1).arg(tag);
              return false;
            }
            const int take = tagIndex == tags.size() ? actualParagraphs.size() - sourceParagraph : 1;
            QStringList textParts; QString html;
            for (int offset = 0; offset < take; ++offset) {
              textParts << actualParagraphs[sourceParagraph + offset].text;
              html += htmlFromParagraph(actualParagraphs[sourceParagraph + offset]);
            }
            bool extracted = false;
            (*values)[tag] = valueForTag(expectedParagraph.text, textParts.join('\n'), tag, &extracted);
            (*htmlValues)[tag] = html;
            if (!extracted) {
              if (diagnostic) *diagnostic = QString("ligne %1, cellule %2 : la zone %3 ne respecte pas son texte fixe").arg(row + 1).arg(column + 1).arg(tag);
              return false;
            }
            sourceParagraph += take;
          }
        }
        continue;
      }
      for (const QString &tag : tags) {
        bool extracted = false;
        const QString value = valueForTag(expected.text, actual.text, tag, &extracted);
        if (!extracted) {
          if (diagnostic) *diagnostic = QString("ligne %1, cellule %2 : la zone %3 ne respecte pas son texte fixe").arg(row + 1).arg(column + 1).arg(tag);
          return false;
        }
        (*values)[tag] = value;
        (*htmlValues)[tag] = expected.text.trimmed().compare("{{" + tag + "}}", Qt::CaseInsensitive) == 0 ? actual.html : value.toHtmlEscaped();
      }
    }
  }
  return true;
}

bool matchTableCandidate(const WordTable &pattern, const WordTable &source,
                         QMap<QString, QString> *values,
                         QMap<QString, QString> *htmlValues,
                         QList<DocxImportDiagnostic> *diagnostics) {
  int fixedLabels = 0, matchedLabels = 0, requiredValues = 0;
  QSet<int> usedRows;
  for (int patternRow = 0; patternRow < pattern.rows.size(); ++patternRow) {
    const auto &expectedRow = pattern.rows[patternRow];
    int bestRow = -1, bestScore = -1;
    for (int sourceRow = 0; sourceRow < source.rows.size(); ++sourceRow) {
      if (usedRows.contains(sourceRow)) continue;
      int score = 0;
      for (int column = 0; column < qMin(expectedRow.size(), source.rows[sourceRow].size()); ++column) {
        const WordCell &expected = expectedRow[column];
        if (tagsIn(expected.text).isEmpty() && !expected.text.trimmed().isEmpty() &&
            normalized(expected.text) == normalized(source.rows[sourceRow][column].text))
          score += 3;
        else if (!tagsIn(expected.text).isEmpty())
          ++score;
      }
      if (score > bestScore) { bestScore = score; bestRow = sourceRow; }
    }
    if (bestRow < 0) continue;
    usedRows.insert(bestRow);
    const auto &actualRow = source.rows[bestRow];
    for (int column = 0; column < expectedRow.size(); ++column) {
      const WordCell &expected = expectedRow[column];
      const QStringList tags = tagsIn(expected.text);
      if (tags.isEmpty()) {
        if (expected.text.trimmed().isEmpty()) continue;
        ++fixedLabels;
        const QString found = column < actualRow.size() ? actualRow[column].text : QString();
        if (normalized(expected.text) == normalized(found)) ++matchedLabels;
        else diagnostics->append({QString("ligne %1, cellule %2").arg(bestRow + 1).arg(column + 1),
                                  {}, QString("Libellé « %1 » attendu, « %2 » trouvé")
                                          .arg(expected.text, found), false});
        continue;
      }
      for (const QString &tag : tags) {
        if (column >= actualRow.size()) {
          diagnostics->append({QString("ligne %1, cellule %2").arg(bestRow + 1).arg(column + 1),
                                tag, "Cellule absente", true});
          continue;
        }
        bool extracted = false;
        const QString value = valueForTag(expected.text, actualRow[column].text, tag, &extracted);
        if (!extracted) {
          // A candidate remains editable: retain the complete cell even if its
          // fixed prefix or suffix differs from the model.
          (*values)[tag] = actualRow[column].text.trimmed();
          (*htmlValues)[tag] = actualRow[column].html;
          diagnostics->append({QString("ligne %1, cellule %2").arg(bestRow + 1).arg(column + 1),
                                tag, "Le texte fixe entourant la valeur diffère du gabarit", false});
        } else {
          (*values)[tag] = value;
          (*htmlValues)[tag] = expected.text.trimmed().compare("{{" + tag + "}}", Qt::CaseInsensitive) == 0
                                   ? actualRow[column].html : value.toHtmlEscaped();
        }
        if ((tag == "REQ_CODE" || tag == "REQ_TITLE" || tag == "REQ_DESCRIPTION") &&
            !(*values)[tag].trimmed().isEmpty())
          ++requiredValues;
      }
    }
  }
  const bool labelsCompatible = fixedLabels == 0 ? source.rows.size() >= pattern.rows.size()
                                                  : matchedLabels * 2 >= fixedLabels;
  return requiredValues >= 2 && labelsCompatible;
}
QString extractMarker(const QString &templatePath, const QString &sourcePath,
                      const QString &marker) {
  QStringList parts{"word/document.xml"};
  QZipReader templateZip(templatePath);
  for (const auto &entry : templateZip.fileInfoList())
    if (entry.isFile && entry.filePath.endsWith(".xml") &&
        (entry.filePath.startsWith("word/header") || entry.filePath.startsWith("word/footer")))
      parts << entry.filePath;
  for (const QString &part : parts) {
    const QString templateXml = QString::fromUtf8(templateZip.fileData(part));
    if (!visible(templateXml).contains(marker, Qt::CaseInsensitive)) continue;
    const auto templateParagraphs = paragraphs(templateXml);
    const auto sourceParagraphs = paragraphs(QString::fromUtf8(partData(sourcePath, part)));
    for (int index = 0; index < templateParagraphs.size(); ++index) {
      if (!templateParagraphs[index].text.contains(marker, Qt::CaseInsensitive) || index >= sourceParagraphs.size()) continue;
      bool ok = false;
      const QString value = valueForTag(templateParagraphs[index].text, sourceParagraphs[index].text,
                                        marker.mid(2, marker.size() - 4), &ok);
      if (ok) return value;
    }
  }
  return {};
}
QString coreTitle(const QString &path) {
  const QString xml = QString::fromUtf8(partData(path, "docProps/core.xml"));
  auto match = QRegularExpression("<dc:title(?:\\s[^>]*)?>([\\s\\S]*?)</dc:title>", QRegularExpression::CaseInsensitiveOption).match(xml);
  return match.hasMatch() ? decode(match.captured(1)).trimmed() : QString();
}
QString customReference(const QString &path) {
  const QString xml = QString::fromUtf8(partData(path, "docProps/custom.xml"));
  QRegularExpression property("<property[^>]*name=\"Reference\"[^>]*>([\\s\\S]*?)</property>", QRegularExpression::CaseInsensitiveOption);
  auto match = property.match(xml);
  if (!match.hasMatch()) return {};
  QString value = match.captured(1);
  value.remove(QRegularExpression("<[^>]+>"));
  return decode(value).trimmed();
}
QStringList lines(const QString &value) {
  return value.split(QRegularExpression("[\\r\\n;]+"), Qt::SkipEmptyParts);
}
int lookup(QSqlDatabase db, const QString &table, const QStringList &columns,
           const QString &value) {
  if (value.trimmed().isEmpty()) return -1;
  for (const QString &column : columns) {
    QSqlQuery query(db);
    query.prepare(QString("SELECT ID FROM %1 WHERE UPPER(TRIM(%2))=? LIMIT 1").arg(table, column));
    query.addBindValue(normalized(value));
    if (query.exec() && query.next()) return query.value(0).toInt();
  }
  return -1;
}
}

DocxImportService::DocxImportService(QString connectionName)
    : m_connectionName(std::move(connectionName)) {}

DocxImportValidation DocxImportService::validateTemplate(const QString &path) const {
  DocxImportValidation result;
  QString error; const QString xml = documentXml(path, &error);
  if (xml.isEmpty()) { result.errors << error; return result; }
  const QString text = visible(xml);
  auto count = [&](const QString &tag) { return text.count(tag, Qt::CaseInsensitive); };
  if (count(ReqBegin) != 1 || count(ReqEnd) != 1)
    result.errors << "Le gabarit doit contenir exactement un début et une fin de bloc d'exigence.";
  const int begin = text.indexOf(ReqBegin, 0, Qt::CaseInsensitive);
  const int end = text.indexOf(ReqEnd, 0, Qt::CaseInsensitive);
  if (begin >= end) result.errors << "Les bornes du bloc d'exigence sont absentes ou inversées.";
  const QString block = begin >= 0 && end > begin ? text.mid(begin, end - begin) : QString();
  for (const QString &required : {QString("REQ_CODE"), QString("REQ_TITLE"), QString("REQ_DESCRIPTION")})
    if (!block.contains("{{" + required + "}}", Qt::CaseInsensitive))
      result.errors << "La balise {{" + required + "}} est obligatoire dans le bloc d'exigence.";
  for (const QString &tag : ReqTags)
    if (block.count("{{" + tag + "}}", Qt::CaseInsensitive) > 1)
      result.errors << "La balise {{" + tag + "}} apparaît plusieurs fois.";
  for (const auto &pair : {qMakePair(VerBegin, VerEnd), qMakePair(RelBegin, RelEnd)}) {
    const int starts = count(pair.first), stops = count(pair.second);
    if (starts != stops || starts > 1) result.errors << "Les sous-blocs répétables sont absents ou mal appariés.";
    if (starts == 1) {
      const int a = text.indexOf(pair.first), b = text.indexOf(pair.second);
      if (a < begin || b > end || a >= b) result.errors << "Un sous-bloc est mal imbriqué dans le bloc d'exigence.";
    }
  }
  if (!block.contains("{{REQ_TYPE}}")) result.warnings << "Le type ne sera pas importé.";
  if (!block.contains("{{REQ_STATUS}}")) result.warnings << "Le statut ne sera pas importé.";
  result.valid = result.errors.isEmpty();
  return result;
}

DocxImportPreview DocxImportService::preview(const QString &sourcePath,
                                             const QString &templatePath) const {
  DocxImportPreview result;
  const auto validation = validateTemplate(templatePath);
  result.errors = validation.errors; result.warnings = validation.warnings;
  if (!validation.valid) return result;
  QString error; const QString templateXml = documentXml(templatePath, &error);
  const QString sourceXml = documentXml(sourcePath, &error);
  if (sourceXml.isEmpty()) { result.errors << error; return result; }
  result.documentTitle = extractMarker(templatePath, sourcePath, "{{GESIER_DOCUMENT_TITLE}}");
  result.documentReference = extractMarker(templatePath, sourcePath, "{{GESIER_REFERENCE}}");
  if (result.documentTitle.isEmpty()) result.documentTitle = coreTitle(sourcePath);
  if (result.documentReference.isEmpty()) result.documentReference = customReference(sourcePath);

  int prototypeStart = -1, prototypeEnd = -1;
  QRegularExpression paragraphExpression("<w:p(?:\\s[^>]*)?>[\\s\\S]*?</w:p>", QRegularExpression::CaseInsensitiveOption);
  auto templateParagraphIterator = paragraphExpression.globalMatch(templateXml);
  while (templateParagraphIterator.hasNext()) {
    const auto paragraph = templateParagraphIterator.next();
    const QString text = visible(paragraph.captured());
    if (text.contains(ReqBegin, Qt::CaseInsensitive)) prototypeStart = paragraph.capturedEnd();
    if (prototypeStart >= 0 && text.contains(ReqEnd, Qt::CaseInsensitive)) { prototypeEnd = paragraph.capturedStart(); break; }
  }
  if (prototypeStart < 0 || prototypeEnd <= prototypeStart) {
    result.errors << "Le prototype d'exigence est vide."; return result;
  }
  const QString prototypeXml = templateXml.mid(prototypeStart, prototypeEnd - prototypeStart);
  WordTable tablePattern;
  for (const WordTable &candidate : tables(prototypeXml)) {
    const QString text = visible(candidate.xml);
    if (text.contains("{{REQ_CODE}}", Qt::CaseInsensitive) &&
        text.contains("{{REQ_TITLE}}", Qt::CaseInsensitive) &&
        text.contains("{{REQ_DESCRIPTION}}", Qt::CaseInsensitive)) {
      tablePattern = candidate; break;
    }
  }
  const QString styles = stylesXml(sourcePath);
  const auto items = bodyItems(sourceXml, styles);
  QStringList chapterStack;
  int ordinal = 0;
  QString firstTableDiagnostic;
  auto appendRequirement = [&](const QMap<QString, QString> &values,
                               const QMap<QString, QString> &htmlValues,
                               DocxImportRecognition recognition = DocxImportRecognition::Conformant,
                               const QList<DocxImportDiagnostic> &details = {}) {
    DocxImportRequirement imported; imported.ordinal = ++ordinal;
    imported.chapterPath = chapterStack;
    imported.location = QString("Exigence %1 — %2").arg(ordinal).arg(chapterStack.join(" / "));
    imported.record.code = values.value("REQ_CODE").trimmed();
    imported.record.title = values.value("REQ_TITLE").trimmed();
    imported.record.description = htmlValues.value("REQ_DESCRIPTION");
    imported.type = values.value("REQ_TYPE").trimmed();
    imported.status = values.value("REQ_STATUS").trimmed();
    imported.record.source = values.value("REQ_SOURCE").trimmed();
    imported.productTreeCodes = lines(values.value("REQ_PRODUCT_TREES"));
    imported.configurationCodes = lines(values.value("REQ_APPLICABILITY"));
    imported.recognition = recognition;
    imported.selected = recognition == DocxImportRecognition::Conformant;
    imported.detailedDiagnostics = details;
    for (const auto &detail : details)
      imported.diagnostics << detail.location + " : " + detail.message;
    for (const QString &line : lines(values.value("REQ_VERIFICATIONS"))) {
      RequirementVerification verification;
      const QStringList fields = line.split('|');
      verification.level = fields.value(1).trimmed();
      verification.procedure = fields.value(2).trimmed();
      verification.means = fields.value(3).trimmed();
      verification.verdict = fields.value(4).trimmed();
      verification.redmine = fields.value(5).trimmed();
      verification.comment = fields.value(6).trimmed();
      verification.methodId = -1; verification.id = -1;
      imported.verificationMethods << fields.value(0).trimmed();
      imported.record.verifications << verification;
    }
    for (const QString &line : lines(values.value("REQ_RELATIONS"))) {
      auto match = QRegularExpression("^(Dépend de|Dérive de|Enfant de)\\s+([^ —(]+)(?:\\s+—[^()]*)?(?:\\s*\\((.*)\\))?$").match(line.trimmed());
      if (match.hasMatch())
        imported.relations << DocxImportRelation{match.captured(1), match.captured(1).startsWith("Enfant") ? "IN" : "OUT", match.captured(2), match.captured(3)};
    }
    if (imported.record.code.isEmpty()) imported.diagnostics << "Code absent";
    if (imported.record.title.isEmpty()) imported.diagnostics << "Titre absent";
    result.requirements << imported;
  };

  if (!tablePattern.rows.isEmpty()) {
    for (const BodyItem &item : items) {
      if (item.kind == BodyItem::ParagraphItem) {
        if (item.paragraph.outline > 0) {
          chapterStack = chapterStack.mid(0, item.paragraph.outline - 1);
          chapterStack << item.paragraph.text;
        }
        continue;
      }
      QMap<QString, QString> values, htmlValues; QString diagnostic;
      if (matchTable(tablePattern, item.table, &values, &htmlValues, &diagnostic))
        appendRequirement(values, htmlValues);
      else {
        QList<DocxImportDiagnostic> details;
        QMap<QString, QString> candidateValues, candidateHtml;
        if (matchTableCandidate(tablePattern, item.table, &candidateValues, &candidateHtml, &details)) {
          details.prepend({QString("Exigence candidate %1").arg(ordinal + 1), {},
                           "La structure diffère du gabarit et doit être vérifiée", false});
          appendRequirement(candidateValues, candidateHtml, DocxImportRecognition::Candidate, details);
        } else if (firstTableDiagnostic.isEmpty() && item.table.rows.size() >= tablePattern.rows.size()) {
          firstTableDiagnostic = diagnostic;
        }
      }
    }
  } else {
    // Preserve paragraph-based templates while using the table-aware matcher
    // whenever the requirement prototype is tabular.
    const auto templateParagraphs = paragraphs(prototypeXml);
    const auto sourceParagraphs = paragraphs(sourceXml, styles);
    for (int start = 0; start + templateParagraphs.size() <= sourceParagraphs.size(); ++start) {
      QMap<QString, QString> values, htmlValues; bool matches = true; int tagged = 0;
      for (int index = 0; index < templateParagraphs.size(); ++index) {
        const QStringList tags = tagsIn(templateParagraphs[index].text);
        if (tags.isEmpty()) {
          if (!templateParagraphs[index].text.isEmpty() && normalized(templateParagraphs[index].text) != normalized(sourceParagraphs[start + index].text)) { matches = false; break; }
          continue;
        }
        ++tagged;
        for (const QString &tag : tags) {
          bool ok = false; values[tag] = valueForTag(templateParagraphs[index].text, sourceParagraphs[start + index].text, tag, &ok);
          htmlValues[tag] = htmlFromParagraph(sourceParagraphs[start + index]);
          if (!ok) { matches = false; break; }
        }
      }
      if (matches && tagged >= 3) { appendRequirement(values, htmlValues); start += templateParagraphs.size() - 1; }
    }
  }
  if (result.requirements.isEmpty()) {
    QString message = "Aucun bloc d'exigence conforme au gabarit n'a été détecté.";
    if (!firstTableDiagnostic.isEmpty()) message += " Première divergence : " + firstTableDiagnostic + ".";
    result.errors << message;
  }
  if (sourceXml.contains("<w:drawing") || sourceXml.contains("<w:object"))
    result.warnings << "Les images et objets incorporés ne sont pas importés.";
  if (sourceXml.contains("<w:tbl")) result.warnings << "Les tableaux contenus dans les descriptions sont aplatis en texte.";
  return result;
}

RequirementResult DocxImportService::importPreview(const DocxImportPreview &preview,
                                                   const DocxImportOptions &options) const {
  if (!preview.valid()) return RequirementResult::failure(preview.errors.join('\n'));
  int selectedCount = 0;
  for (const auto &item : preview.requirements) {
    if (!item.selected) continue;
    ++selectedCount;
    if (item.record.code.trimmed().isEmpty() || item.record.title.trimmed().isEmpty() ||
        QTextDocumentFragment::fromHtml(item.record.description).toPlainText().trimmed().isEmpty())
      return RequirementResult::failure(QString("%1 : code, titre et description sont obligatoires.").arg(item.location));
  }
  if (selectedCount == 0) return RequirementResult::failure("Aucune exigence sélectionnée.");
  QSqlDatabase db = QSqlDatabase::database(m_connectionName);
  if (!db.transaction()) return RequirementResult::failure(db.lastError().text());
  auto fail = [&](const QString &message) { db.rollback(); return RequirementResult::failure(message); };
  int documentId = options.documentId;
  if (documentId < 0) {
    if (options.documentReference.trimmed().isEmpty() || options.documentTitle.trimmed().isEmpty())
      return fail("La référence et le titre de la nouvelle spécification sont obligatoires.");
    QSqlQuery create(db);
    create.prepare("INSERT INTO DOCUMENT(PT_ID,TYPE,REFERENCE,TITLE,DESCRIPTION) VALUES(?,COALESCE((SELECT ID FROM DOC_TYPE WHERE UPPER(TYPE)='SP' LIMIT 1),(SELECT ID FROM DOC_TYPE ORDER BY ID LIMIT 1)),?,?,?)");
    create.addBindValue(options.primaryPtId); create.addBindValue(options.documentReference);
    create.addBindValue(options.documentTitle); create.addBindValue(options.documentDescription);
    if (!create.exec()) return fail(create.lastError().text());
    documentId = create.lastInsertId().toInt();
    QSqlQuery reference(db);
    reference.prepare("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(?,?,1)");
    reference.addBindValue(documentId); reference.addBindValue(options.documentReference);
    if (!reference.exec()) return fail(reference.lastError().text());
  }
  RequirementService requirements(m_connectionName); DocumentService documents(m_connectionName);
  ProductTreeService productTrees(m_connectionName);
  QMap<QString, int> chapters, idsByCode; int created = 0, updated = 0;
  const auto existingNodes = documents.nodes(documentId);
  QMap<int, QString> existingPaths;
  for (int pass = 0; pass <= existingNodes.size(); ++pass)
    for (const auto &node : existingNodes) {
      if (node.type != "CHAPTER" || existingPaths.contains(node.id)) continue;
      if (node.parentId >= 0 && !existingPaths.contains(node.parentId)) continue;
      const QString path = (node.parentId < 0 ? QString() : existingPaths.value(node.parentId)) + "/" + normalized(node.title);
      existingPaths[node.id] = path; chapters[path] = node.id;
    }
  for (const auto &item : preview.requirements) {
    if (!item.selected) continue;
    RequirementRecord record = item.record;
    const QString context = QString("%1 [%2] : ").arg(item.location, record.code);
    QSqlQuery existing(db); existing.prepare("SELECT ID FROM REQUIREMENT WHERE CODE=?"); existing.addBindValue(record.code);
    const bool exists = existing.exec() && existing.next();
    if (exists) {
      record = requirements.get(existing.value(0).toInt());
      if (options.updateDuplicates && item.importAction != "keep") {
        record.title = item.record.title; record.description = item.record.description;
        record.source = item.record.source;
      }
    } else { record.primaryPtId = options.primaryPtId; record.ptIds = {options.primaryPtId}; }
    if (!item.type.isEmpty()) { const int id = lookup(db, "REQ_TYPE", {"TYPE", "CODE"}, item.type); if (id < 0) return fail(context + "type inconnu : " + item.type); record.typeId = id; }
    if (!item.status.isEmpty()) { const int id = lookup(db, "REQ_STATUS", {"STATUS", "SHORTCUT"}, item.status); if (id < 0) return fail(context + "statut inconnu : " + item.status); record.statusId = id; }
    if (!item.productTreeCodes.isEmpty()) {
      record.ptIds.clear(); record.primaryPtId = -1;
      for (const QString &code : item.productTreeCodes) {
        int id = lookup(db, "PT", {"SEGMENT", "NAME"}, code);
        if (id < 0) {
          QSqlQuery all("SELECT ID FROM PT WHERE ARCHIVED=0", db);
          while (all.next()) if (normalized(productTrees.fullCode(all.value(0).toInt())) == normalized(code)) { id = all.value(0).toInt(); break; }
        }
        if (id < 0) return fail(context + "Product Tree inconnu : " + code);
        record.ptIds << id; if (record.primaryPtId < 0) record.primaryPtId = id;
      }
    }
    record.configurationIds.clear();
    for (const QString &code : item.configurationCodes) { const int id = lookup(db, "CONFIGURATION", {"CODE", "LABEL"}, code); if (id < 0) return fail(context + "configuration inconnue : " + code); record.configurationIds << id; }
    record.verifications.clear();
    for (int verificationIndex = 0; verificationIndex < item.record.verifications.size(); ++verificationIndex) {
      auto verification = item.record.verifications[verificationIndex];
      const QString method = item.verificationMethods.value(verificationIndex);
      verification.methodId = lookup(db, "REQ_METHOD", {"METHOD"}, method);
      if (verification.methodId < 0) return fail(context + "méthode inconnue : " + method);
      record.verifications << verification;
    }
    RequirementResult saved = item.importAction == "keep" && exists
                                  ? RequirementResult::successResult("Exigence conservée.", record.id)
                                  : requirements.save(record, false);
    if (!saved.success) return fail(saved.message);
    idsByCode[normalized(record.code)] = saved.id; exists ? ++updated : ++created;
    int parent = -1; QString key;
    for (const QString &chapter : item.chapterPath) {
      key += "/" + normalized(chapter);
      if (!chapters.contains(key)) { const auto added = documents.addChapter(documentId, parent, chapter, false); if (!added.success) return fail(added.message); chapters[key] = added.id; }
      parent = chapters[key];
    }
    const auto placed = documents.placeRequirement(documentId, parent, saved.id, false); if (!placed.success) return fail(placed.message);
  }
  // Relations are deliberately written after all requirements have IDs.
  for (const auto &item : preview.requirements) {
    if (!item.selected) continue;
    const int owner = idsByCode.value(normalized(item.record.code), -1);
    QMap<int, QSet<QString>> synchronizedTypes;
    for (const auto &relation : item.relations) {
      const QString code = relation.type.startsWith("Dépend") ? "DEPENDS_ON" : relation.type.startsWith("Dérive") ? "DERIVES_FROM" : "DECOMPOSE";
      const int typeId = lookup(db, "REQUIREMENT_RELATION_TYPE", {"CODE", "LABEL"}, code);
      if (typeId >= 0) synchronizedTypes[typeId].insert(relation.direction);
    }
    for (auto type = synchronizedTypes.cbegin(); type != synchronizedTypes.cend(); ++type) {
      for (const QString &direction : type.value()) {
        QSqlQuery clear(db);
        clear.prepare(direction == "IN"
                          ? "DELETE FROM REQUIREMENT_RELATION WHERE TARGET_REQ_ID=? AND TYPE_ID=?"
                          : "DELETE FROM REQUIREMENT_RELATION WHERE SOURCE_REQ_ID=? AND TYPE_ID=?");
        clear.addBindValue(owner); clear.addBindValue(type.key());
        if (!clear.exec()) return fail(clear.lastError().text());
      }
    }
    for (const auto &relation : item.relations) {
      int other = idsByCode.value(normalized(relation.otherCode), -1);
      if (other < 0) { QSqlQuery q(db); q.prepare("SELECT ID FROM REQUIREMENT WHERE UPPER(CODE)=?"); q.addBindValue(normalized(relation.otherCode)); if (q.exec() && q.next()) other = q.value(0).toInt(); }
      const QString typeCode = relation.type.startsWith("Dépend") ? "DEPENDS_ON" : relation.type.startsWith("Dérive") ? "DERIVES_FROM" : "DECOMPOSE";
      const int typeId = lookup(db, "REQUIREMENT_RELATION_TYPE", {"CODE", "LABEL"}, typeCode);
      if (owner < 0 || other < 0 || typeId < 0) return fail("Relation non résolue pour " + item.record.code + " vers " + relation.otherCode);
      QSqlQuery add(db); add.prepare("INSERT OR IGNORE INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID,COMMENT) VALUES(?,?,?,?)");
      add.addBindValue(relation.direction == "IN" ? other : owner);
      add.addBindValue(relation.direction == "IN" ? owner : other);
      add.addBindValue(typeId); add.addBindValue(relation.comment);
      if (!add.exec()) return fail(add.lastError().text());
    }
  }
  if (!db.commit()) return fail(db.lastError().text());
  return RequirementResult::successResult(QString("Import Word terminé : %1 créée(s), %2 mise(s) à jour.").arg(created).arg(updated), documentId);
}
