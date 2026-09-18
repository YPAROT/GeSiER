#include "requirementimportdialog.h"

#include "producttreeservice.h"
#include "requirementservice.h"
#include "importvalidationdialog.h"
#include "documentservice.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QSettings>
#include <QTextDocumentFragment>
#include <algorithm>

namespace {
enum Field { Code, Title, Description, Type, Source, Applicability,
             Methods, VerificationLevel, Method1, Level1, Method2, Level2, Section,
             FieldCount };
const QStringList fieldNames = {
    "Code *", "Titre *", "Descriptif *", "Type", "Source",
    "Applicabilité / configurations", "Méthodes (liste)", "Niveau de vérification",
    "Méthode 1", "Niveau 1", "Méthode 2", "Niveau 2", "Section / chapitre"};

QString normalized(QString value) {
  return value.simplified().toUpper();
}

QStringList splitValues(const QString &value) {
  return value.split(QRegularExpression("[;,;|,]+"), Qt::SkipEmptyParts);
}
}

RequirementImportDialog::RequirementImportDialog(const QString &connectionName,
                                                 QWidget *parent)
    : QDialog(parent), m_connection(connectionName) {
  setWindowTitle("Importer une spécification CSV/XLSX");
  resize(1050, 760);
  setWindowTitle("Importer une spécification");
  auto *choose = new QPushButton("Choisir un fichier CSV/XLSX/DOCX…");
  m_fileLabel = new QLabel("Aucun fichier sélectionné");
  m_sheet = new QComboBox;
  m_separator = new QComboBox;
  m_separator->addItem("Détection automatique", QString());
  m_separator->addItem("Point-virgule ( ; )", ";");
  m_separator->addItem("Virgule ( , )", ",");
  m_separator->addItem("Tabulation", "\t");
  m_encoding = new QComboBox;
  m_encoding->addItem("Détection automatique", int(TabularEncoding::Auto));
  m_encoding->addItem("UTF-8", int(TabularEncoding::Utf8));
  m_encoding->addItem("Windows-1252", int(TabularEncoding::Windows1252));
  m_headerRow = new QSpinBox;
  m_headerRow->setMinimum(1);
  m_headerRow->setMaximum(10000);
  m_ignoredRows = new QSpinBox;
  m_ignoredRows->setRange(0, 1000000);
  m_external = new QCheckBox("Import d'exigences externes (préfixe EXTERNAL-)");
  m_rememberMapping = new QCheckBox("Mémoriser ces correspondances pour cet en-tête");
  m_duplicates = new QComboBox;
  m_duplicates->addItem("Mettre à jour", "update");
  m_duplicates->addItem("Ignorer", "skip");
  m_duplicates->addItem("Refuser l'import", "reject");
  m_documentMode = new QComboBox;
  m_documentMode->addItem("Créer une nouvelle spécification", "new");
  m_documentMode->addItem("Ajouter à une spécification existante", "existing");
  m_existingDocument = new QComboBox;
  m_documentReference = new QLineEdit;
  m_documentTitle = new QLineEdit;
  m_documentDescription = new QLineEdit;
  m_wordTemplate = new QLineEdit(QSettings().value("Requirements/lastWordImportTemplate").toString());
  auto *chooseWordTemplate = new QPushButton("Choisir…");
  auto *validateWordTemplate = new QPushButton("Valider");
  auto *templateRow = new QHBoxLayout;
  templateRow->addWidget(m_wordTemplate); templateRow->addWidget(chooseWordTemplate); templateRow->addWidget(validateWordTemplate);
  m_primaryPt = new QComboBox;
  ProductTreeService productTrees(m_connection);
  QSqlQuery pts("SELECT ID,COALESCE(NAME,DESCRIPTION,SEGMENT) FROM PT WHERE ARCHIVED=0 ORDER BY POSITION,ID",
                QSqlDatabase::database(m_connection));
  while (pts.next()) m_primaryPt->addItem(productTrees.fullCode(pts.value(0).toInt()) + " — " + pts.value(1).toString(), pts.value(0));
  QSqlQuery documents("SELECT ID,COALESCE(REFERENCE,'')||' — '||TITLE FROM DOCUMENT ORDER BY TITLE",
                      QSqlDatabase::database(m_connection));
  while (documents.next())
    m_existingDocument->addItem(documents.value(1).toString(), documents.value(0));
  auto *form = new QFormLayout;
  form->addRow(choose, m_fileLabel);
  form->addRow("Gabarit d'import Word", templateRow);
  form->addRow("Product Tree principal", m_primaryPt);
  form->addRow("Onglet", m_sheet);
  form->addRow("Séparateur CSV", m_separator);
  form->addRow("Encodage CSV", m_encoding);
  form->addRow("Ligne d'en-tête", m_headerRow);
  form->addRow("Lignes ignorées après l'en-tête", m_ignoredRows);
  form->addRow(QString(), m_external);
  form->addRow(QString(), m_rememberMapping);
  form->addRow("Codes existants", m_duplicates);
  form->addRow("Document cible", m_documentMode);
  form->addRow("Spécification existante", m_existingDocument);
  form->addRow("Référence de la nouvelle spécification", m_documentReference);
  form->addRow("Titre de la nouvelle spécification", m_documentTitle);
  form->addRow("Description", m_documentDescription);
  m_preview = new QTableWidget;
  m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_mapping = new QTableWidget(FieldCount, 3);
  m_mapping->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_mapping->setHorizontalHeaderLabels({"Champ GeSiER", "Colonne source", "Transformation"});
  m_mapping->horizontalHeader()->setStretchLastSection(true);
  for (int row = 0; row < FieldCount; ++row)
    m_mapping->setItem(row, 0, new QTableWidgetItem(fieldNames[row]));
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
  auto *importButton = buttons->addButton("Valider et importer",
                                         QDialogButtonBox::AcceptRole);
  auto *layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(new QLabel("Aperçu"));
  layout->addWidget(m_preview, 2);
  layout->addWidget(new QLabel("Correspondance des colonnes"));
  layout->addWidget(m_mapping, 1);
  layout->addWidget(buttons);
  connect(choose, &QPushButton::clicked, this,
          &RequirementImportDialog::chooseFile);
  connect(chooseWordTemplate, &QPushButton::clicked, this, [this] {
    const QString path = QFileDialog::getOpenFileName(this, "Gabarit d'import Word", m_wordTemplate->text(), "Documents Word (*.docx)");
    if (!path.isEmpty()) { m_wordTemplate->setText(path); QSettings().setValue("Requirements/lastWordImportTemplate", path); if (m_wordMode) loadWordSource(); }
  });
  connect(validateWordTemplate, &QPushButton::clicked, this, [this] {
    DocxImportService service(m_connection); const auto result = service.validateTemplate(m_wordTemplate->text());
    QString message = result.valid ? "Gabarit valide." : result.errors.join('\n');
    if (!result.warnings.isEmpty()) message += "\n\nAvertissements :\n" + result.warnings.join('\n');
    result.valid ? QMessageBox::information(this, "Gabarit Word", message) : QMessageBox::warning(this, "Gabarit Word", message);
  });
  connect(m_sheet, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &RequirementImportDialog::refreshPreview);
  connect(m_headerRow, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &RequirementImportDialog::refreshPreview);
  connect(m_ignoredRows, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &RequirementImportDialog::refreshPreview);
  connect(m_separator, &QComboBox::currentIndexChanged, this,
          [this] { if (!m_filePath.isEmpty()) loadSource(); });
  connect(m_encoding, &QComboBox::currentIndexChanged, this,
          [this] { if (!m_filePath.isEmpty()) loadSource(); });
  connect(importButton, &QPushButton::clicked, this,
          &RequirementImportDialog::runImport);
  connect(buttons, &QDialogButtonBox::rejected, this,
          &QDialog::reject);
  connect(m_documentMode, &QComboBox::currentIndexChanged, this, [this] {
    const bool existing = m_documentMode->currentData().toString() == "existing";
    m_existingDocument->setEnabled(existing);
    m_documentReference->setEnabled(!existing);
    m_documentTitle->setEnabled(!existing);
    m_documentDescription->setEnabled(!existing);
  });
  m_existingDocument->setEnabled(false);
}

void RequirementImportDialog::chooseFile() {
  const QString fileName = QFileDialog::getOpenFileName(
      this, "Spécification", QString(),
      "Spécifications (*.xlsx *.csv *.docx);;Document Word (*.docx);;Classeur Excel (*.xlsx);;CSV (*.csv)");
  if (fileName.isEmpty())
    return;
  m_filePath = fileName;
  m_wordMode = fileName.endsWith(".docx", Qt::CaseInsensitive);
  if (m_wordMode) loadWordSource(); else loadSource();
}

void RequirementImportDialog::loadWordSource() {
  if (m_wordTemplate->text().trimmed().isEmpty()) {
    QMessageBox::information(this, "Import Word", "Sélectionnez le gabarit DOCX décrivant la structure des exigences.");
    return;
  }
  QSettings().setValue("Requirements/lastWordImportTemplate", m_wordTemplate->text());
  DocxImportService service(m_connection);
  m_wordPreview = service.preview(m_filePath, m_wordTemplate->text());
  m_fileLabel->setText(m_filePath);
  m_sheet->setEnabled(false); m_separator->setEnabled(false); m_encoding->setEnabled(false);
  m_headerRow->setEnabled(false); m_ignoredRows->setEnabled(false); m_mapping->setEnabled(false);
  m_preview->clear(); m_preview->setColumnCount(7);
  m_preview->setHorizontalHeaderLabels({"N°", "Chapitre", "Code", "Titre", "Type/Statut", "Description", "Relations"});
  m_preview->setRowCount(m_wordPreview.requirements.size());
  for (int row = 0; row < m_wordPreview.requirements.size(); ++row) {
    const auto &requirement = m_wordPreview.requirements[row];
    const QString metadata = requirement.type + (requirement.status.isEmpty() ? QString() : " / " + requirement.status);
    const QString description = QTextDocumentFragment::fromHtml(requirement.record.description).toPlainText();
    const QStringList values{QString::number(requirement.ordinal), requirement.chapterPath.join(" / "), requirement.record.code,
                             requirement.record.title, metadata, description, QString::number(requirement.relations.size())};
    for (int column = 0; column < values.size(); ++column) m_preview->setItem(row, column, new QTableWidgetItem(values[column]));
  }
  m_preview->resizeColumnsToContents();
  QStringList diagnostics = m_wordPreview.errors;
  for (const QString &warning : m_wordPreview.warnings) diagnostics << "Avertissement : " + warning;
  if (!diagnostics.isEmpty()) QMessageBox::warning(this, "Aperçu Word", diagnostics.join('\n'));
}

void RequirementImportDialog::loadSource() {
  if (m_filePath.isEmpty()) return;
  QString error;
  TabularReadOptions options;
  const QString separator = m_separator->currentData().toString();
  if (!separator.isEmpty()) options.separator = separator.at(0);
  options.encoding = TabularEncoding(m_encoding->currentData().toInt());
  m_sheets = TabularService::read(m_filePath, options, &error).sheets;
  if (m_sheets.isEmpty()) {
    QMessageBox::warning(this, "Import tabulaire", error);
    return;
  }
  m_fileLabel->setText(m_filePath);
  m_sheet->clear();
  for (const TabularSheet &sheet : m_sheets)
    m_sheet->addItem(sheet.name);
  if (m_sheets.first().rows.isEmpty())
    QMessageBox::information(
        this, "Import tabulaire",
        "L'onglet sélectionné ne contient aucune cellule lisible. Vous pouvez "
        "essayer un autre onglet du classeur.");
  refreshPreview();
}

void RequirementImportDialog::refreshPreview() {
  if (m_sheet->currentIndex() < 0 || m_sheet->currentIndex() >= m_sheets.size())
    return;
  const auto &rows = m_sheets[m_sheet->currentIndex()].rows;
  m_headerRow->setMaximum(qMax(1, rows.size()));
  const int header = m_headerRow->value() - 1;
  if (header < 0 || header >= rows.size())
    return;
  int columns = rows[header].size();
  const int firstData = header + 1 + m_ignoredRows->value();
  for (int row = firstData; row < qMin(rows.size(), firstData + 20); ++row)
    columns = qMax(columns, rows[row].size());
  m_preview->setColumnCount(columns);
  m_preview->setHorizontalHeaderLabels(rows[header]);
  m_preview->setRowCount(qMax(0, qMin(20, rows.size() - firstData)));
  for (int row = 0; row < m_preview->rowCount(); ++row)
    for (int column = 0; column < columns; ++column)
      m_preview->setItem(row, column, new QTableWidgetItem(
          column < rows[firstData + row].size()
              ? rows[firstData + row][column]
              : QString()));
  rebuildMappings();
}

void RequirementImportDialog::rebuildMappings() {
  if (m_sheet->currentIndex() < 0)
    return;
  const QStringList headers =
      m_sheets[m_sheet->currentIndex()].rows.value(m_headerRow->value() - 1);
  const QList<QStringList> aliases = {
      {"CODE", "ID", "IDENTIFIANT", "REFERENCE", "RÉFÉRENCE"},
      {"TITRE", "TITLE", "NOM"},
      {"DESCRIPTION", "DESCRIPTIF", "TEXT"}, {"TYPE", "NATURE"},
      {"SOURCE", "ORIGIN", "ORIGINE"},
      {"APPLICABILITE", "APPLICABILITY", "CONFIGURATION", "MODELE"},
      {"METHODES", "METHODS", "METHODE DE VERIFICATION"},
      {"NIVEAU", "VERIFICATION LEVEL"}, {"METHODE 1", "METHOD 1"},
      {"NIVEAU 1", "LEVEL 1"}, {"METHODE 2", "METHOD 2"},
      {"NIVEAU 2", "LEVEL 2"}, {"SECTION", "CHAPTER", "CHAPITRE"}};
  const QString profileScope = "requirements-import-" +
                               normalized(headers.join("|"));
  const TabularProfile savedProfile = TabularService::loadProfile(profileScope);
  for (int field = 0; field < FieldCount; ++field) {
    auto *combo = new QComboBox;
    combo->addItem("— Non importé —", -1);
    for (int column = 0; column < headers.size(); ++column)
      combo->addItem(headers[column], column);
    QString savedHeader;
    TabularTransform savedTransform = TabularTransform::Trim;
    for (const auto &mapping : savedProfile.mappings)
      if (mapping.target == QString::number(field)) {
        savedHeader = mapping.sourceHeader;
        savedTransform = mapping.transform;
      }
    int selectedColumn = savedHeader.isEmpty() ? -1 : headers.indexOf(savedHeader);
    if (selectedColumn < 0)
      for (int column = 0; column < headers.size(); ++column)
        if (aliases[field].contains(normalized(headers[column]))) {
          selectedColumn = column;
          break;
        }
    if (selectedColumn >= 0)
      combo->setCurrentIndex(selectedColumn + 1);
    m_mapping->setCellWidget(field, 1, combo);
    auto *transformation = new QComboBox;
    transformation->addItem("Nettoyer les espaces", int(TabularTransform::Trim));
    transformation->addItem("Conserver", int(TabularTransform::None));
    transformation->addItem("MAJUSCULES", int(TabularTransform::Uppercase));
    transformation->addItem("minuscules", int(TabularTransform::Lowercase));
    transformation->setCurrentIndex(qMax(0, transformation->findData(int(savedTransform))));
    m_mapping->setCellWidget(field, 2, transformation);
  }
}

int RequirementImportDialog::mappedColumn(int field) const {
  auto *combo = qobject_cast<QComboBox *>(m_mapping->cellWidget(field, 1));
  return combo ? combo->currentData().toInt() : -1;
}

QString RequirementImportDialog::mappedValue(const QStringList &row,
                                              int field) const {
  const int column = mappedColumn(field);
  QString value = column >= 0 && column < row.size() ? row[column] : QString();
  auto *combo = qobject_cast<QComboBox *>(m_mapping->cellWidget(field, 2));
  const auto transformation = combo ? TabularTransform(combo->currentData().toInt())
                                    : TabularTransform::Trim;
  return TabularService::transform(value, transformation);
}

int RequirementImportDialog::resolveLevel(const QString &value, int occurrences,
                                          QMap<QString, int> &decisions,
                                          bool *rejected) {
  const QString key = normalized(value);
  if (key.isEmpty())
    return -1;
  if (decisions.contains(key))
    return decisions[key];
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  ProductTreeService service(m_connection);
  QList<int> matches;
  QSqlQuery query("SELECT ID,SEGMENT,DESCRIPTION FROM PT WHERE ARCHIVED=0", db);
  while (query.next()) {
    const int id = query.value(0).toInt();
    if (normalized(query.value(1).toString()) == key ||
        normalized(query.value(2).toString()) == key ||
        normalized(service.fullCode(id)) == key)
      matches << id;
  }
  if (matches.size() == 1) {
    decisions[key] = matches.first();
    return matches.first();
  }
  QMessageBox box(this);
  box.setWindowTitle("Niveau de vérification non résolu");
  box.setText(QString("Le niveau « %1 » n'a pas de correspondance unique. "
                      "Il apparaît dans %2 ligne(s) de vérification.")
                  .arg(value).arg(occurrences));
  auto *choice = new QComboBox(&box);
  choice->addItem("— Ignorer ce niveau —", -1);
  QSqlQuery pts("SELECT ID,DESCRIPTION FROM PT WHERE ARCHIVED=0 ORDER BY POSITION,SEGMENT", db);
  while (pts.next()) {
    const int id = pts.value(0).toInt();
    choice->addItem(service.fullCode(id) + " — " + pts.value(1).toString(), id);
  }
  box.layout()->addWidget(choice);
  auto *applyAll = new QCheckBox("Appliquer à toutes les occurrences de cette valeur", &box);
  box.setCheckBox(applyAll);
  box.addButton("Continuer", QMessageBox::AcceptRole);
  box.addButton("Rejeter les lignes", QMessageBox::RejectRole);
  box.exec();
  if (box.clickedButton() && box.buttonRole(box.clickedButton()) == QMessageBox::RejectRole) {
    *rejected = true;
    return -1;
  }
  const int result = choice->currentData().toInt();
  if (applyAll->isChecked())
    decisions[key] = result;
  return result;
}

void RequirementImportDialog::runImport() {
  if (m_wordMode) { runWordImport(); return; }
  if (m_sheet->currentIndex() < 0)
    return;
  if (mappedColumn(Code) < 0 || mappedColumn(Title) < 0 ||
      mappedColumn(Description) < 0) {
    QMessageBox::warning(this, "Import Excel",
                         "Code, titre et descriptif doivent être associés.");
    return;
  }
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  QSqlQuery rootQuery("SELECT ID FROM PT WHERE PARENT IS NULL AND ARCHIVED=0", db);
  if (!rootQuery.next()) {
    QMessageBox::warning(this, "Import Excel",
                         "Le projet doit posséder une racine Product Tree.");
    return;
  }
  const int rootId = rootQuery.value(0).toInt();
  const auto &rows = m_sheets[m_sheet->currentIndex()].rows;
  const int firstDataRow = m_headerRow->value() + m_ignoredRows->value();
  QMap<QString, int> levelDecisions;
  QMap<QString, int> levelOccurrences;
  struct PreparedRequirement {
    RequirementRecord record;
    QString section;
    QStringList configurationsToCreate;
    bool save = true;
  };
  QList<PreparedRequirement> records;
  QStringList errors;
  for (int index = firstDataRow; index < rows.size(); ++index) {
    const QStringList &row = rows[index];
    const int listedMethods = splitValues(mappedValue(row, Methods)).size();
    const QString common = normalized(mappedValue(row, VerificationLevel));
    if (!common.isEmpty())
      levelOccurrences[common] += listedMethods;
    for (const auto &pair :
         {qMakePair(Method1, Level1), qMakePair(Method2, Level2)}) {
      if (!mappedValue(row, pair.first).isEmpty()) {
        const QString level = normalized(mappedValue(row, pair.second));
        if (!level.isEmpty())
          ++levelOccurrences[level];
      }
    }
  }
  auto lookup = [&](const QString &table, const QString &labelColumn,
                    const QString &raw) {
    if (raw.isEmpty()) return -1;
    QSqlQuery query(db);
    query.prepare(QString("SELECT ID FROM %1 WHERE UPPER(TRIM(%2))=? LIMIT 1")
                      .arg(table, labelColumn));
    query.addBindValue(normalized(raw));
    return query.exec() && query.next() ? query.value(0).toInt() : -1;
  };
  auto catalogChoices = [&](const QString &sql) {
    QList<QPair<int, QString>> result;
    QSqlQuery query(sql, db);
    while (query.next())
      result << qMakePair(query.value(0).toInt(), query.value(1).toString());
    return result;
  };
  const auto typeChoices = catalogChoices(
      "SELECT ID,COALESCE(CODE||' — ','')||TYPE FROM REQ_TYPE ORDER BY ID");
  const auto methodChoices = catalogChoices(
      "SELECT ID,METHOD FROM REQ_METHOD ORDER BY ID");
  const auto configurationChoices = catalogChoices(
      "SELECT ID,CODE||' — '||LABEL FROM CONFIGURATION WHERE ACTIVE=1 ORDER BY CODE");
  QMap<QString, ImportConflict> groupedConflicts;
  auto addConflict = [&](const QString &prefix, const QString &category,
                         const QString &raw, int line,
                         const QList<QPair<int, QString>> &choices) {
    const QString key = prefix + "|" + normalized(raw);
    ImportConflict &conflict = groupedConflicts[key];
    conflict.key = key;
    conflict.category = category;
    conflict.sourceValue = raw;
    conflict.choices = choices;
    ++conflict.occurrences;
    conflict.lines.insert(line);
  };
  for (int index = firstDataRow; index < rows.size(); ++index) {
    const QStringList &row = rows[index];
    const QString type = mappedValue(row, Type);
    if (!type.isEmpty() && lookup("REQ_TYPE", "CODE", type) < 0 &&
        lookup("REQ_TYPE", "TYPE", type) < 0)
      addConflict("TYPE", "Type inconnu", type, index + 1, typeChoices);
    const QString applicability = mappedValue(row, Applicability);
    for (const QString &configuration : splitValues(applicability))
      if (lookup("CONFIGURATION", "CODE", configuration) < 0 &&
          lookup("CONFIGURATION", "LABEL", configuration) < 0)
        addConflict("CONFIGURATION", "Configuration inconnue", configuration,
                    index + 1, configurationChoices);
    QStringList importedMethods =
        splitValues(mappedValue(row, Methods));
    for (Field field : {Method1, Method2}) {
      const QString method = mappedValue(row, field);
      if (!method.isEmpty())
        importedMethods << method;
    }
    for (const QString &method : importedMethods)
      if (lookup("REQ_METHOD", "METHOD", method) < 0)
        addConflict("METHOD", "Méthode inconnue", method, index + 1,
                    methodChoices);
  }
  QMap<QString, int> catalogDecisions;
  if (!groupedConflicts.isEmpty()) {
    ImportValidationDialog validation(groupedConflicts.values(), this);
    if (validation.exec() != QDialog::Accepted)
      return;
    catalogDecisions = validation.decisions();
  }
  auto resolvedCatalogId = [&](const QString &prefix, const QString &raw,
                               int exactId) {
    return catalogDecisions.value(prefix + "|" + normalized(raw), exactId);
  };
  QList<ImportDuplicate> duplicateRows;
  for (int index = firstDataRow; index < rows.size(); ++index) {
    const QStringList &row = rows[index];
    QString code = mappedValue(row, Code);
    if (m_external->isChecked() && !code.startsWith("EXTERNAL-", Qt::CaseInsensitive))
      code.prepend("EXTERNAL-");
    QSqlQuery existing(db);
    existing.prepare("SELECT TITLE,COALESCE(DESCRIPTION,'') FROM REQUIREMENT WHERE CODE=?");
    existing.addBindValue(code);
    if (existing.exec() && existing.next())
      duplicateRows << ImportDuplicate{code, existing.value(0).toString(),
                                       mappedValue(row, Title),
                                       existing.value(1).toString(),
                                       mappedValue(row, Description)};
  }
  QMap<QString, QString> duplicateDecisions;
  if (!duplicateRows.isEmpty()) {
    ImportDuplicateDialog duplicates(duplicateRows,
                                     m_duplicates->currentData().toString(), this);
    if (duplicates.exec() != QDialog::Accepted)
      return;
    duplicateDecisions = duplicates.decisions();
  }
  for (int index = firstDataRow; index < rows.size(); ++index) {
    const QStringList &row = rows[index];
    QString code = mappedValue(row, Code);
    const bool codeWasEmpty = code.isEmpty();
    const QString title = mappedValue(row, Title);
    const QString description = mappedValue(row, Description);
    if (code.isEmpty() && title.isEmpty() && description.isEmpty())
      continue;
    if (m_external->isChecked() && !code.startsWith("EXTERNAL-", Qt::CaseInsensitive))
      code.prepend("EXTERNAL-");
    QSqlQuery existing(db);
    existing.prepare("SELECT ID FROM REQUIREMENT WHERE CODE=?");
    existing.addBindValue(code);
    const bool exists = existing.exec() && existing.next();
    if (codeWasEmpty || (!exists && (title.isEmpty() || description.isEmpty()))) {
      errors << QString("Ligne %1 : code absent ou titre/descriptif absent pour une création.")
                    .arg(index + 1);
      continue;
    }
    RequirementService service(m_connection);
    RequirementRecord record;
    bool rowRejected = false;
    bool saveRecord = true;
    QStringList configurationsToCreate;
    if (exists) {
      const QString policy = duplicateDecisions.value(
          code, m_duplicates->currentData().toString());
      if (policy == "reject") {
        errors << QString("Ligne %1 : le code %2 existe déjà.").arg(index + 1).arg(code);
        continue;
      }
      record = service.get(existing.value(0).toInt());
      if (policy == "skip")
        continue;
      if (policy == "keep")
        saveRecord = false;
    } else {
      record.code = code;
      record.primaryPtId = rootId;
      record.ptIds = {rootId};
    }
    if (saveRecord) {
      record.title = title.isEmpty() ? record.title : title;
      record.description = description.isEmpty() ? record.description : description;
    }
    const QString source = mappedValue(row, Source);
    if (saveRecord && !source.isEmpty()) record.source = source;
    const QString type = mappedValue(row, Type);
    if (saveRecord && !type.isEmpty()) {
      int typeId = lookup("REQ_TYPE", "CODE", type);
      if (typeId < 0) typeId = lookup("REQ_TYPE", "TYPE", type);
      typeId = resolvedCatalogId("TYPE", type, typeId);
      if (typeId == -2)
      {
        errors << QString("Ligne %1 : rejetée à cause du type « %2 ».")
                      .arg(index + 1).arg(type);
        rowRejected = true;
      }
      else if (typeId >= 0)
        record.typeId = typeId;
    }
    const QString applicability = mappedValue(row, Applicability);
    if (saveRecord && !applicability.isEmpty()) {
      QList<int> importedConfigurations;
      for (const QString &configuration : splitValues(applicability)) {
        int configurationId = lookup("CONFIGURATION", "CODE", configuration);
        if (configurationId < 0)
          configurationId = lookup("CONFIGURATION", "LABEL", configuration);
        configurationId = resolvedCatalogId("CONFIGURATION", configuration,
                                            configurationId);
        if (configurationId == -2)
        {
          errors << QString("Ligne %1 : rejetée à cause de la configuration « %2 ».")
                        .arg(index + 1).arg(configuration);
          rowRejected = true;
        }
        else if (configurationId == -3)
          configurationsToCreate << configuration;
        else if (configurationId >= 0 &&
                 !importedConfigurations.contains(configurationId))
          importedConfigurations << configurationId;
      }
      if (!importedConfigurations.isEmpty())
        record.configurationIds = importedConfigurations;
    }
    QStringList methods = saveRecord ? splitValues(mappedValue(row, Methods)) : QStringList();
    QStringList levels;
    const QString commonLevel = mappedValue(row, VerificationLevel);
    for (int i = 0; i < methods.size(); ++i) levels << commonLevel;
    for (const auto &pair :
         {qMakePair(Method1, Level1), qMakePair(Method2, Level2)}) {
      const QString method = mappedValue(row, pair.first);
      if (!method.isEmpty()) { methods << method; levels << mappedValue(row, pair.second); }
    }
    QList<RequirementVerification> importedVerifications;
    for (int i = 0; i < methods.size(); ++i) {
      int methodId = lookup("REQ_METHOD", "METHOD", methods[i]);
      methodId = resolvedCatalogId("METHOD", methods[i], methodId);
      if (methodId == -2) {
        errors << QString("Ligne %1 : rejetée à cause de la méthode « %2 ».")
                      .arg(index + 1).arg(methods[i]);
        rowRejected = true;
        break;
      }
      if (methodId < 0)
        continue;
      bool rejected = false;
      const QString levelValue = levels.value(i);
      const int levelId = resolveLevel(
          levelValue, levelOccurrences.value(normalized(levelValue), 1),
          levelDecisions, &rejected);
      if (rejected) {
        errors << QString("Ligne %1 rejetée à cause du niveau « %2 ».").arg(index + 1).arg(levels.value(i));
        rowRejected = true;
        break;
      }
      RequirementVerification verification;
      verification.methodId = methodId;
      verification.levelPtId = levelId;
      if (!std::any_of(importedVerifications.begin(), importedVerifications.end(),
                       [methodId](const auto &item) { return item.methodId == methodId; }))
        importedVerifications << verification;
    }
    if (!importedVerifications.isEmpty())
      record.verifications = importedVerifications;
    if (!rowRejected && !record.code.isEmpty())
      records << PreparedRequirement{record, mappedValue(row, Section),
                                     configurationsToCreate, saveRecord};
  }
  if (!errors.isEmpty()) {
    if (!ImportValidationDialog::askToImportValidRows(errors, records.size(),
                                                       this))
      return;
  }
  if (records.isEmpty()) {
    QMessageBox::information(
        this, "Import Excel",
        "Aucune ligne valide ne peut être importée ou rattachée. "
        "La spécification ne sera pas créée.");
    return;
  }
  if (m_rememberMapping->isChecked()) {
    const QStringList headers = rows.value(m_headerRow->value() - 1);
    TabularProfile profile;
    profile.name = "default";
    profile.sheetName = m_sheet->currentText();
    profile.headerRow = m_headerRow->value() - 1;
    profile.ignoredRows = m_ignoredRows->value();
    profile.format = m_filePath.endsWith(".csv", Qt::CaseInsensitive)
                         ? TabularFormat::Csv : TabularFormat::Xlsx;
    const QString separator = m_separator->currentData().toString();
    if (!separator.isEmpty()) profile.separator = separator.at(0);
    profile.encoding = TabularEncoding(m_encoding->currentData().toInt());
    for (int field = 0; field < FieldCount; ++field) {
      const int column = mappedColumn(field);
      auto *transformCombo = qobject_cast<QComboBox *>(m_mapping->cellWidget(field, 2));
      profile.mappings << TabularColumnMapping{
          QString::number(field),
          column >= 0 && column < headers.size() ? headers[column] : QString(),
          {}, column,
          transformCombo ? TabularTransform(transformCombo->currentData().toInt())
                         : TabularTransform::Trim};
    }
    TabularService::saveProfile("requirements-import-" + normalized(headers.join("|")),
                                profile);
  }
  if (!db.transaction()) {
    QMessageBox::warning(this, "Import Excel", db.lastError().text());
    return;
  }
  int documentId = -1;
  QString documentReference;
  if (m_documentMode->currentData().toString() == "existing") {
    documentId = m_existingDocument->currentData().toInt();
    QSqlQuery document(db);
    document.prepare("SELECT COALESCE(REFERENCE,'') FROM DOCUMENT WHERE ID=?");
    document.addBindValue(documentId);
    if (document.exec() && document.next())
      documentReference = document.value(0).toString();
    else {
      db.rollback();
      QMessageBox::warning(this, "Import tabulaire", "La spécification cible n'existe plus.");
      return;
    }
  } else {
    documentReference = m_documentReference->text().trimmed();
    if (documentReference.isEmpty() || m_documentTitle->text().trimmed().isEmpty()) {
      db.rollback();
      QMessageBox::warning(this, "Import Excel", "La référence et le titre de la spécification sont obligatoires.");
      return;
    }
    QSqlQuery document(db);
    document.prepare("INSERT INTO DOCUMENT(PT_ID,TYPE,REFERENCE,TITLE,DESCRIPTION) VALUES(?,COALESCE((SELECT ID FROM DOC_TYPE WHERE UPPER(TYPE)='SP' LIMIT 1),(SELECT ID FROM DOC_TYPE ORDER BY ID LIMIT 1)),?,?,?)");
    document.addBindValue(rootId);
    document.addBindValue(documentReference);
    document.addBindValue(m_documentTitle->text().trimmed());
    document.addBindValue(m_documentDescription->text());
    if (!document.exec()) {
      const QString message = document.lastError().text(); db.rollback();
      QMessageBox::warning(this, "Import Excel", message); return;
    }
    documentId = document.lastInsertId().toInt();
    QSqlQuery reference(db);
    reference.prepare("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(?,?,1)");
    reference.addBindValue(documentId); reference.addBindValue(documentReference);
    if (!reference.exec()) { const QString message=reference.lastError().text(); db.rollback(); QMessageBox::warning(this,"Import Excel",message); return; }
  }
  RequirementService service(m_connection);
  DocumentService documents(m_connection);
  QMap<QString, int> chapterIds;
  QMap<QString, int> chapterTitles;
  for (const DocumentNodeRecord &node : documents.nodes(documentId))
    if (node.type == "CHAPTER")
      chapterTitles[normalized(node.title)] = node.id;
  QMap<QString, int> createdConfigurations;
  int createdCount = 0, updatedCount = 0, attachedCount = 0;
  for (PreparedRequirement prepared : records) {
    for (const QString &configuration : prepared.configurationsToCreate) {
      const QString key = normalized(configuration);
      if (!createdConfigurations.contains(key)) {
        QSqlQuery createConfiguration(db);
        createConfiguration.prepare("INSERT INTO CONFIGURATION(CODE,LABEL) VALUES(?,?)");
        createConfiguration.addBindValue(configuration.trimmed());
        createConfiguration.addBindValue(configuration.trimmed());
        if (!createConfiguration.exec()) {
          const QString message = createConfiguration.lastError().text();
          db.rollback(); QMessageBox::warning(this, "Import Excel", message); return;
        }
        createdConfigurations[key] = createConfiguration.lastInsertId().toInt();
      }
      if (!prepared.record.configurationIds.contains(createdConfigurations[key]))
        prepared.record.configurationIds << createdConfigurations[key];
    }
    if (prepared.save && prepared.record.source.trimmed().isEmpty())
      prepared.record.source = documentReference;
    int requirementId = prepared.record.id;
    if (prepared.save) {
      const bool creation = prepared.record.id < 0;
      const RequirementResult result = service.save(prepared.record, false);
      if (!result.success) {
        db.rollback();
        QMessageBox::warning(this, "Import Excel", result.message +
                             "\nAucune modification n'a été conservée.");
        return;
      }
      requirementId = result.id;
      if (creation) ++createdCount; else ++updatedCount;
    } else {
      ++attachedCount;
    }
    int parentId = -1;
    QString section = prepared.section.trimmed();
    QStringList path;
    if (section.contains('/')) {
      path = section.split('/', Qt::SkipEmptyParts);
    } else if (!section.isEmpty()) {
      const QRegularExpressionMatch match = QRegularExpression("^([0-9]+(?:\\.[0-9]+)*)\\s*[-–:]?\\s*(.*)$").match(section);
      const QString key = match.hasMatch() ? match.captured(1) : normalized(section);
      const QString title = match.hasMatch() && !match.captured(2).trimmed().isEmpty() ? match.captured(2).trimmed() : section;
      const QString parentKey = key.contains('.') ? key.left(key.lastIndexOf('.')) : QString();
      parentId = chapterIds.value(parentKey, -1);
      if (!chapterIds.contains(key) && chapterTitles.contains(normalized(title)))
        chapterIds[key] = chapterTitles[normalized(title)];
      if (!chapterIds.contains(key)) {
        const RequirementResult chapter = documents.addChapter(documentId, parentId, title, false);
        if (!chapter.success) { db.rollback(); QMessageBox::warning(this,"Import Excel",chapter.message); return; }
        chapterIds[key] = chapter.id;
      }
      parentId = chapterIds[key];
    }
    QString cumulative;
    for (QString title : path) {
      title = title.trimmed();
      cumulative += "/" + normalized(title);
      if (!chapterIds.contains(cumulative)) {
        const RequirementResult chapter = documents.addChapter(documentId, parentId, title, false);
        if (!chapter.success) { db.rollback(); QMessageBox::warning(this,"Import Excel",chapter.message); return; }
        chapterIds[cumulative] = chapter.id;
      }
      parentId = chapterIds[cumulative];
    }
    const RequirementResult placement = documents.placeRequirement(documentId, parentId, requirementId, false);
    if (!placement.success) {
      db.rollback();
      QMessageBox::warning(this, "Import Excel", placement.message +
                           "\nAucune modification n'a été conservée.");
      return;
    }
  }
  if (!db.commit()) {
    db.rollback();
    QMessageBox::warning(this, "Import Excel", db.lastError().text());
    return;
  }
  QMessageBox::information(
      this, "Import tabulaire",
      QString("Import terminé : %1 créée(s), %2 mise(s) à jour, %3 conservée(s) et rattachée(s).")
          .arg(createdCount).arg(updatedCount).arg(attachedCount));
  emit imported();
  accept();
}

void RequirementImportDialog::runWordImport() {
  if (!m_wordPreview.valid()) {
    QMessageBox::warning(this, "Import Word", m_wordPreview.errors.join('\n'));
    return;
  }
  DocxImportPreview selected = m_wordPreview;
  QList<ImportDuplicate> duplicateRows;
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  for (const auto &item : selected.requirements) {
    QSqlQuery existing(db); existing.prepare("SELECT TITLE,COALESCE(DESCRIPTION,'') FROM REQUIREMENT WHERE CODE=?"); existing.addBindValue(item.record.code);
    if (existing.exec() && existing.next()) duplicateRows << ImportDuplicate{item.record.code, existing.value(0).toString(), item.record.title,
      QTextDocumentFragment::fromHtml(existing.value(1).toString()).toPlainText(), QTextDocumentFragment::fromHtml(item.record.description).toPlainText()};
  }
  QMap<QString, QString> decisions;
  if (!duplicateRows.isEmpty()) {
    ImportDuplicateDialog dialog(duplicateRows, m_duplicates->currentData().toString(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    decisions = dialog.decisions();
  }
  for (int index = selected.requirements.size() - 1; index >= 0; --index) {
    const QString action = decisions.value(selected.requirements[index].record.code, "update");
    if (action == "skip" || action == "reject") selected.requirements.removeAt(index);
    else selected.requirements[index].importAction = action;
  }
  if (selected.requirements.isEmpty()) { QMessageBox::information(this, "Import Word", "Aucune exigence à importer."); return; }
  DocxImportOptions options;
  options.primaryPtId = m_primaryPt->currentData().toInt();
  options.updateDuplicates = true;
  if (m_documentMode->currentData().toString() == "existing") options.documentId = m_existingDocument->currentData().toInt();
  else { options.documentReference = m_documentReference->text().trimmed(); options.documentTitle = m_documentTitle->text().trimmed(); options.documentDescription = m_documentDescription->text(); }
  DocxImportService service(m_connection); const auto result = service.importPreview(selected, options);
  if (!result.success) { QMessageBox::critical(this, "Import Word", result.message); return; }
  QMessageBox::information(this, "Import Word", result.message); emit imported(); accept();
}
