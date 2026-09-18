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
QString tagName(const QString &text) {
  auto m = QRegularExpression("\\{\\{([A-Z0-9_]+)\\}\\}").match(text.trimmed());
  return m.hasMatch() && m.captured() == text.trimmed() ? m.captured(1) : QString();
}
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
QStringList lines(const QString &value) {
  return value.split(QRegularExpression("[\\r\\n;]+"), Qt::SkipEmptyParts);
}
QString normalized(QString value) { return value.simplified().toUpper(); }
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
  const auto templateParagraphs = paragraphs(templateXml);
  int first = -1, last = -1;
  for (int i = 0; i < templateParagraphs.size(); ++i) {
    if (templateParagraphs[i].text.contains(ReqBegin, Qt::CaseInsensitive)) first = i + 1;
    if (templateParagraphs[i].text.contains(ReqEnd, Qt::CaseInsensitive)) { last = i; break; }
  }
  if (first < 0 || last <= first) { result.errors << "Le prototype d'exigence est vide."; return result; }
  QList<Paragraph> pattern = templateParagraphs.mid(first, last - first);
  // Markers for nested records are structural only; aggregate tags remain a
  // compatible representation for documents produced by the current exporter.
  pattern.erase(std::remove_if(pattern.begin(), pattern.end(), [](const Paragraph &p) {
    return p.text == VerBegin || p.text == VerEnd || p.text == RelBegin || p.text == RelEnd;
  }), pattern.end());
  const QString styles = stylesXml(sourcePath);
  const auto source = paragraphs(sourceXml, styles);
  QStringList chapterStack;
  int ordinal = 0;
  for (int start = 0; start + pattern.size() <= source.size();) {
    if (source[start].outline > 0) {
      chapterStack = chapterStack.mid(0, source[start].outline - 1);
      chapterStack << source[start].text; ++start; continue;
    }
    bool matches = true; int tagged = 0;
    for (int i = 0; i < pattern.size(); ++i) {
      const QString tag = tagName(pattern[i].text);
      if (!tag.isEmpty()) { ++tagged; continue; }
      if (normalized(pattern[i].text) != normalized(source[start + i].text)) { matches = false; break; }
    }
    if (!matches || tagged < 3) { ++start; continue; }
    DocxImportRequirement imported; imported.ordinal = ++ordinal;
    imported.chapterPath = chapterStack;
    imported.location = QString("Exigence %1 — %2").arg(ordinal).arg(chapterStack.join(" / "));
    for (int i = 0; i < pattern.size(); ++i) {
      const QString tag = tagName(pattern[i].text);
      const Paragraph &value = source[start + i];
      if (tag == "REQ_CODE") imported.record.code = value.text;
      else if (tag == "REQ_TITLE") imported.record.title = value.text;
      else if (tag == "REQ_DESCRIPTION") imported.record.description = htmlFromParagraph(value);
      else if (tag == "REQ_TYPE") imported.type = value.text;
      else if (tag == "REQ_STATUS") imported.status = value.text;
      else if (tag == "REQ_SOURCE") imported.record.source = value.text;
      else if (tag == "REQ_PRODUCT_TREES") imported.productTreeCodes = lines(value.text);
      else if (tag == "REQ_APPLICABILITY") imported.configurationCodes = lines(value.text);
      else if (tag == "REQ_VERIFICATIONS") {
        for (const QString &line : lines(value.text)) {
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
      } else if (tag == "REQ_RELATIONS") {
        for (const QString &line : lines(value.text)) {
          auto m = QRegularExpression("^(Dépend de|Dérive de|Enfant de)\\s+([^ —(]+)(?:\\s+—[^()]*)?(?:\\s*\\((.*)\\))?$").match(line.trimmed());
          if (m.hasMatch()) imported.relations << DocxImportRelation{m.captured(1), m.captured(1).startsWith("Enfant") ? "IN" : "OUT", m.captured(2), m.captured(3)};
        }
      }
    }
    if (imported.record.code.isEmpty() || imported.record.title.isEmpty())
      result.errors << imported.location + " : code ou titre absent.";
    else result.requirements << imported;
    start += pattern.size();
  }
  if (result.requirements.isEmpty()) result.errors << "Aucun bloc d'exigence conforme au gabarit n'a été détecté.";
  if (sourceXml.contains("<w:drawing") || sourceXml.contains("<w:object"))
    result.warnings << "Les images et objets incorporés ne sont pas importés.";
  if (sourceXml.contains("<w:tbl")) result.warnings << "Les tableaux contenus dans les descriptions sont aplatis en texte.";
  return result;
}

RequirementResult DocxImportService::importPreview(const DocxImportPreview &preview,
                                                   const DocxImportOptions &options) const {
  if (!preview.valid()) return RequirementResult::failure(preview.errors.join('\n'));
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
    RequirementRecord record = item.record;
    QSqlQuery existing(db); existing.prepare("SELECT ID FROM REQUIREMENT WHERE CODE=?"); existing.addBindValue(record.code);
    const bool exists = existing.exec() && existing.next();
    if (exists) {
      record = requirements.get(existing.value(0).toInt());
      if (options.updateDuplicates && item.importAction != "keep") {
        record.title = item.record.title; record.description = item.record.description;
        record.source = item.record.source;
      }
    } else { record.primaryPtId = options.primaryPtId; record.ptIds = {options.primaryPtId}; }
    if (!item.type.isEmpty()) { const int id = lookup(db, "REQ_TYPE", {"TYPE", "CODE"}, item.type); if (id < 0) return fail("Type inconnu : " + item.type); record.typeId = id; }
    if (!item.status.isEmpty()) { const int id = lookup(db, "REQ_STATUS", {"STATUS", "SHORTCUT"}, item.status); if (id < 0) return fail("Statut inconnu : " + item.status); record.statusId = id; }
    if (!item.productTreeCodes.isEmpty()) {
      record.ptIds.clear(); record.primaryPtId = -1;
      for (const QString &code : item.productTreeCodes) {
        int id = lookup(db, "PT", {"SEGMENT", "NAME"}, code);
        if (id < 0) {
          QSqlQuery all("SELECT ID FROM PT WHERE ARCHIVED=0", db);
          while (all.next()) if (normalized(productTrees.fullCode(all.value(0).toInt())) == normalized(code)) { id = all.value(0).toInt(); break; }
        }
        if (id < 0) return fail("Product Tree inconnu : " + code);
        record.ptIds << id; if (record.primaryPtId < 0) record.primaryPtId = id;
      }
    }
    record.configurationIds.clear();
    for (const QString &code : item.configurationCodes) { const int id = lookup(db, "CONFIGURATION", {"CODE", "LABEL"}, code); if (id < 0) return fail("Configuration inconnue : " + code); record.configurationIds << id; }
    record.verifications.clear();
    for (int verificationIndex = 0; verificationIndex < item.record.verifications.size(); ++verificationIndex) {
      auto verification = item.record.verifications[verificationIndex];
      const QString method = item.verificationMethods.value(verificationIndex);
      verification.methodId = lookup(db, "REQ_METHOD", {"METHOD"}, method);
      if (verification.methodId < 0) return fail("Méthode inconnue : " + method);
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
