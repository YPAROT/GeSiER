#include "requirementimportdialog.h"

#include "producttreeservice.h"
#include "requirementservice.h"
#include "importvalidationdialog.h"
#include "documentservice.h"
#include "checkablecombobox.h"
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
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QTextDocumentFragment>
#include <QBrush>
#include <QColor>
#include "markdownutility.h"
#include <algorithm>
#include <functional>

namespace {
enum Field { Code, Title, Description, Type, Source, Applicability,
             Methods, VerificationLevel, Method1, Level1, Method2, Level2,
             Section, TraceRoot, Comments,
             FieldCount };
const QStringList fieldNames = {
    "Code *", "Titre *", "Descriptif *", "Type", "Source",
    "Applicabilité / configurations", "Méthodes (liste)", "Niveau de vérification",
    "Méthode 1", "Niveau 1", "Méthode 2", "Niveau 2",
    "Section / chapitre", "Racine de traçabilité", "Commentaires"};

QString normalized(QString value) {
  return value.simplified().toUpper();
}

QStringList splitValues(const QString &value) {
  return value.split(QRegularExpression("[;,;|,]+"), Qt::SkipEmptyParts);
}

enum WordColumn { WordSelected, WordTraceRoot, WordOrdinal, WordChapter, WordCode,
                  WordTitle, WordRecognition, WordDiagnostic, WordColumnCount };

bool parseTraceRootValueImpl(const QString &raw, bool *value) {
  const QString normalizedValue = normalized(raw);
  if (normalizedValue.isEmpty() || normalizedValue == "NON" ||
      normalizedValue == "FALSE" || normalizedValue == "0") {
    *value = false;
    return true;
  }
  if (normalizedValue == "OUI" || normalizedValue == "TRUE" ||
      normalizedValue == "1" || normalizedValue == "RACINE" ||
      normalizedValue == "ROOT") {
    *value = true;
    return true;
  }
  return false;
}

QStringList editedLines(const QString &value) {
  QStringList result;
  for (const QString &part : value.split(QRegularExpression("[\\r\\n;]+"), Qt::SkipEmptyParts))
    result << part.trimmed();
  return result;
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
  QSqlQuery documents("SELECT D.ID,COALESCE((SELECT R.REFERENCE FROM DOCUMENT_REFERENCE R WHERE R.DOC_ID=D.ID AND R.IS_PRIMARY=1 ORDER BY R.ID LIMIT 1),'')||' — '||D.TITLE FROM DOCUMENT D ORDER BY D.TITLE",
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
  m_importButton = buttons->addButton("Valider et importer",
                                      QDialogButtonBox::AcceptRole);
  m_importButton->setObjectName("importSpecificationButton");
  auto *layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(new QLabel("Aperçu"));
  layout->addWidget(m_preview, 2);
  m_allTraceRoots = new QPushButton("Tout marquer root");
  m_allTraceRoots->setObjectName("allTraceRootsButton");
  m_noTraceRoots = new QPushButton("Tout démarquer root");
  m_noTraceRoots->setObjectName("noTraceRootsButton");
  m_traceRootOverrideLabel = new QLabel("Root : valeurs individuelles / source");
  auto *traceRootActions = new QHBoxLayout;
  traceRootActions->addWidget(m_allTraceRoots);
  traceRootActions->addWidget(m_noTraceRoots);
  traceRootActions->addWidget(m_traceRootOverrideLabel);
  traceRootActions->addStretch();
  layout->addLayout(traceRootActions);
  auto *editWordButton = new QPushButton("Corriger l'exigence sélectionnée…");
  editWordButton->setObjectName("editWordRequirement");
  editWordButton->setVisible(false);
  layout->addWidget(editWordButton);
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
  connect(m_importButton, &QPushButton::clicked, this,
          &RequirementImportDialog::runImport);
  connect(editWordButton, &QPushButton::clicked, this, [this] {
    if (m_wordMode && m_preview->currentRow() >= 0) editWordRequirement(m_preview->currentRow());
  });
  connect(m_preview, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    if (m_wordMode) editWordRequirement(row);
  });
  connect(m_preview, &QTableWidget::itemChanged, this,
          [this](QTableWidgetItem *item) {
            if (!m_wordMode || !item ||
                (item->column() != WordTraceRoot && item->column() != WordSelected) ||
                item->row() < 0 ||
                item->row() >= m_wordPreview.requirements.size())
              return;
            if (item->column() == WordSelected) {
              m_wordPreview.requirements[item->row()].selected =
                  item->checkState() == Qt::Checked;
              updateWordImportAvailability();
              return;
            }
            m_wordPreview.requirements[item->row()].record.traceRoot =
                item->checkState() == Qt::Checked;
            m_traceRootOverride = -1;
            m_traceRootOverrideLabel->setText(
                "Root : valeurs individuelles / source");
          });
  connect(m_allTraceRoots, &QPushButton::clicked, this,
          [this] { setAllTraceRoots(true); });
  connect(m_noTraceRoots, &QPushButton::clicked, this,
          [this] { setAllTraceRoots(false); });
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

bool RequirementImportDialog::parseTraceRootValue(const QString &raw,
                                                  bool *value) {
  return value && parseTraceRootValueImpl(raw, value);
}

void RequirementImportDialog::chooseFile() {
  const QString fileName = QFileDialog::getOpenFileName(
      this, "Spécification", QString(),
      "Spécifications (*.xlsx *.csv *.docx);;Document Word (*.docx);;Classeur Excel (*.xlsx);;CSV (*.csv)");
  if (fileName.isEmpty())
    return;
  m_filePath = fileName;
  m_wordMode = fileName.endsWith(".docx", Qt::CaseInsensitive);
  m_traceRootOverride = -1;
  m_traceRootOverrideLabel->setText("Root : valeurs individuelles / source");
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
  if (m_documentMode->currentData().toString() == "new") {
    if (m_documentReference->text().trimmed().isEmpty())
      m_documentReference->setText(m_wordPreview.documentReference);
    if (m_documentTitle->text().trimmed().isEmpty())
      m_documentTitle->setText(m_wordPreview.documentTitle);
  }
  m_fileLabel->setText(m_filePath);
  m_sheet->setEnabled(false); m_separator->setEnabled(false); m_encoding->setEnabled(false);
  m_headerRow->setEnabled(false); m_ignoredRows->setEnabled(false); m_mapping->setEnabled(false);
  m_mapping->setVisible(false);
  if (auto *button = findChild<QPushButton *>("editWordRequirement")) button->setVisible(true);
  m_preview->clear(); m_preview->setColumnCount(WordColumnCount);
  m_preview->setHorizontalHeaderLabels({"Importer", "Root", "N°", "Chapitre", "Code", "Titre", "Reconnaissance", "Diagnostic"});
  m_preview->setRowCount(m_wordPreview.requirements.size());
  for (int row = 0; row < m_wordPreview.requirements.size(); ++row) refreshWordPreviewRow(row);
  m_preview->horizontalHeader()->setSectionResizeMode(WordDiagnostic, QHeaderView::Stretch);
  m_preview->resizeColumnsToContents();
  updateWordImportAvailability();
  QStringList diagnostics = m_wordPreview.errors;
  for (const QString &warning : m_wordPreview.warnings) diagnostics << "Avertissement : " + warning;
  if (!diagnostics.isEmpty()) QMessageBox::warning(this, "Aperçu Word", diagnostics.join('\n'));
}

void RequirementImportDialog::loadSource() {
  if (m_filePath.isEmpty()) return;
  m_mapping->setVisible(true);
  if (auto *button = findChild<QPushButton *>("editWordRequirement")) button->setVisible(false);
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
  updateWordImportAvailability();
  refreshPreview();
}

void RequirementImportDialog::refreshWordPreviewRow(int row) {
  if (row < 0 || row >= m_wordPreview.requirements.size()) return;
  const QSignalBlocker blocker(m_preview);
  const auto &requirement = m_wordPreview.requirements[row];
  int blocking = 0, warnings = 0;
  for (const auto &detail : requirement.detailedDiagnostics)
    detail.blocking ? ++blocking : ++warnings;
  const QString state = blocking > 0 ? QString::fromUtf8("⛔ %1 erreur(s)").arg(blocking)
                                    : warnings > 0 ? QString::fromUtf8("⚠ %1 avertissement(s)").arg(warnings)
                                                   : QString::fromUtf8("✓ Conforme");
  const QColor background = blocking > 0 ? QColor("#fde7e9")
                                         : warnings > 0 ? QColor("#fff4ce")
                                                        : QColor("#e8f5e9");
  const QStringList values{"", "", QString::number(requirement.ordinal), requirement.chapterPath.join(" / "),
                           requirement.record.code, requirement.record.title,
                           state,
                           requirement.diagnostics.join(" ; ")};
  for (int column = 0; column < WordColumnCount; ++column) {
    auto *cell = m_preview->item(row, column);
    if (!cell) { cell = new QTableWidgetItem; m_preview->setItem(row, column, cell); }
    cell->setText(values[column]);
    cell->setBackground(QBrush(background));
    cell->setToolTip(requirement.diagnostics.join('\n'));
    cell->setFlags(cell->flags() & ~Qt::ItemIsEditable);
    if (column == WordSelected || column == WordTraceRoot) {
      cell->setFlags(cell->flags() | Qt::ItemIsUserCheckable);
      cell->setCheckState(column == WordSelected
                              ? (requirement.selected ? Qt::Checked : Qt::Unchecked)
                              : (requirement.record.traceRoot ? Qt::Checked
                                                              : Qt::Unchecked));
    }
  }
}

void RequirementImportDialog::revalidateWordPreview() {
  DocxImportService(m_connection).validatePreview(&m_wordPreview);
  for (int row = 0; row < m_wordPreview.requirements.size(); ++row)
    refreshWordPreviewRow(row);
  updateWordImportAvailability();
}

void RequirementImportDialog::updateWordImportAvailability() {
  if (!m_importButton) return;
  if (!m_wordMode) { m_importButton->setEnabled(true); return; }
  bool selected = false, blocking = false;
  for (const auto &item : m_wordPreview.requirements) {
    if (!item.selected) continue;
    selected = true;
    for (const auto &detail : item.detailedDiagnostics)
      if (detail.blocking) { blocking = true; break; }
  }
  m_importButton->setEnabled(selected && !blocking && m_wordPreview.valid());
  m_importButton->setToolTip(blocking ? "Corrigez ou décochez les exigences en erreur avant l’import."
                                     : QString());
}

void RequirementImportDialog::setAllTraceRoots(bool traceRoot) {
  m_traceRootOverride = traceRoot ? 1 : 0;
  m_traceRootOverrideLabel->setText(
      traceRoot ? "Root : toutes les exigences" : "Root : aucune exigence");
  if (!m_wordMode)
    return;
  m_preview->blockSignals(true);
  for (int row = 0; row < m_wordPreview.requirements.size(); ++row) {
    m_wordPreview.requirements[row].record.traceRoot = traceRoot;
    if (auto *item = m_preview->item(row, WordTraceRoot))
      item->setCheckState(traceRoot ? Qt::Checked : Qt::Unchecked);
  }
  m_preview->blockSignals(false);
}

void RequirementImportDialog::editWordRequirement(int row) {
  if (row < 0 || row >= m_wordPreview.requirements.size()) return;
  auto &item = m_wordPreview.requirements[row];
  QDialog dialog(this);
  dialog.setWindowTitle(QString("Corriger l'exigence %1").arg(item.ordinal));
  dialog.resize(980, 720);
  auto *tabs = new QTabWidget(&dialog);
  auto *general = new QWidget;
  auto *form = new QFormLayout(general);
  auto *code = new QLineEdit(item.record.code);
  auto *title = new QLineEdit(item.record.title);
  auto *source = new QLineEdit(item.record.source);
  auto catalogCombo = [&](const QString &table, const QString &displayColumn,
                          const QString &valueColumn, const QString &current) {
    auto *combo = new QComboBox;
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->addItem(QString());
    QSqlQuery query(QString("SELECT %1,%2 FROM %3 ORDER BY %1").arg(displayColumn, valueColumn, table),
                    QSqlDatabase::database(m_connection));
    while (query.next()) combo->addItem(query.value(0).toString(), query.value(1));
    combo->setCurrentText(current);
    return combo;
  };
  auto *type = catalogCombo("REQ_TYPE", "TYPE", "ID", item.type);
  auto *status = catalogCombo("REQ_STATUS", "STATUS", "ID", item.status);
  auto selectCatalogValue = [&](QComboBox *combo, const QString &table,
                                const QStringList &columns, const QString &value) {
    for (const QString &column : columns) {
      QSqlQuery query(QSqlDatabase::database(m_connection));
      query.prepare(QString("SELECT ID FROM %1 WHERE UPPER(TRIM(%2))=? LIMIT 1").arg(table, column));
      query.addBindValue(normalized(value));
      if (query.exec() && query.next()) {
        const int index = combo->findData(query.value(0));
        if (index >= 0) combo->setCurrentIndex(index);
        return;
      }
    }
  };
  selectCatalogValue(type, "REQ_TYPE", {"TYPE", "CODE"}, item.type);
  selectCatalogValue(status, "REQ_STATUS", {"STATUS", "SHORTCUT"}, item.status);
  auto *traceRoot = new QCheckBox("Exigence haut niveau / racine de traçabilité");
  traceRoot->setChecked(item.record.traceRoot);
  auto *chapter = new QLineEdit(item.chapterPath.join(" / "));
  ProductTreeService productTreeService(m_connection);
  auto *pts = new CheckableComboBox("Product Trees");
  auto *configs = new CheckableComboBox("Configurations");
  QMap<int, QString> ptCodes, configurationCodes;
  QMap<int, QStringList> ptAliases, configurationAliases;
  QList<int> selectedPts, selectedConfigurations;
  QStringList unknownPts, unknownConfigurations;
  QSqlDatabase editorDb = QSqlDatabase::database(m_connection);
  QSqlQuery ptQuery("SELECT ID,COALESCE(NAME,DESCRIPTION,SEGMENT),SEGMENT,COALESCE(NAME,''),COALESCE(DESCRIPTION,'') FROM PT WHERE ARCHIVED=0 ORDER BY POSITION,ID", editorDb);
  while (ptQuery.next()) {
    const int id = ptQuery.value(0).toInt(); const QString fullCode = productTreeService.fullCode(id);
    ptCodes[id] = fullCode; pts->addItem(fullCode + " — " + ptQuery.value(1).toString(), id);
    ptAliases[id] = {fullCode, ptQuery.value(2).toString(), ptQuery.value(3).toString(), ptQuery.value(4).toString()};
  }
  for (const QString &value : item.productTreeCodes) {
    int found = -1;
    for (auto it = ptAliases.cbegin(); it != ptAliases.cend(); ++it)
      for (const QString &alias : it.value())
        if (normalized(alias) == normalized(value)) { found = it.key(); break; }
    if (found >= 0) selectedPts << found; else unknownPts << value;
  }
  pts->setCheckedIds(selectedPts);
  QSqlQuery configQuery("SELECT ID,CODE,LABEL FROM CONFIGURATION WHERE ACTIVE=1 ORDER BY CODE", editorDb);
  while (configQuery.next()) {
    const int id = configQuery.value(0).toInt(); const QString value = configQuery.value(1).toString();
    configurationCodes[id] = value; configs->addItem(value + " — " + configQuery.value(2).toString(), id);
    configurationAliases[id] = {value, configQuery.value(2).toString()};
  }
  for (const QString &value : item.configurationCodes) {
    int found = -1;
    for (auto it = configurationAliases.cbegin(); it != configurationAliases.cend(); ++it)
      for (const QString &alias : it.value())
        if (normalized(alias) == normalized(value)) { found = it.key(); break; }
    if (found >= 0) selectedConfigurations << found; else unknownConfigurations << value;
  }
  configs->setCheckedIds(selectedConfigurations);
  auto *unknownValues = new QLabel;
  unknownValues->setWordWrap(true);
  QStringList unknownMessages;
  if (!unknownPts.isEmpty()) unknownMessages << "PT inconnus (retirés si vous enregistrez) : " + unknownPts.join(", ");
  if (!unknownConfigurations.isEmpty()) unknownMessages << "Configurations inconnues (retirées si vous enregistrez) : " + unknownConfigurations.join(", ");
  unknownValues->setText(unknownMessages.join('\n'));
  unknownValues->setStyleSheet(unknownMessages.isEmpty() ? QString() : "QLabel{color:#b42318;background:#fde7e9;padding:4px;}");
  const QString originalComments = item.record.comments;
  auto *comments = new QTextEdit(originalComments);
  comments->setObjectName("wordRequirementComments");
  comments->setMaximumHeight(80);
  auto *description = new QTextEdit;
  description->document()->setMarkdown(
      MarkdownUtility::normalizeLegacy(item.record.description),
      QTextDocument::MarkdownDialectGitHub);
  form->addRow("Code *", code); form->addRow("Titre *", title);
  form->addRow("Source", source); form->addRow("Type", type); form->addRow("Statut", status);
  form->addRow(QString(), traceRoot);
  form->addRow("Chapitre (niveaux séparés par /)", chapter);
  form->addRow("Product Trees", pts);
  form->addRow("Configurations", configs);
  form->addRow(QString(), unknownValues);
  form->addRow("Description", description);
  form->addRow("Commentaire", comments);
  tabs->addTab(general, "Exigence");

  auto *verificationPage = new QWidget;
  auto *verificationLayout = new QVBoxLayout(verificationPage);
  std::function<void()> updateEditorDiagnostics;
  auto *verifications = new QTableWidget(0, 7);
  verifications->setObjectName("wordVerificationTable");
  verifications->setHorizontalHeaderLabels({"Méthode", "Niveau", "Procédure", "Redmine", "Moyens", "Verdict", "Commentaire"});
  verifications->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  auto addVerification = [&](const RequirementVerification &verification, const QString &method) {
    const int r = verifications->rowCount(); verifications->insertRow(r);
    auto *methodCombo = new QComboBox; methodCombo->setEditable(true); methodCombo->setInsertPolicy(QComboBox::NoInsert);
    QSqlQuery methods("SELECT METHOD FROM REQ_METHOD ORDER BY METHOD", editorDb);
    while (methods.next()) methodCombo->addItem(methods.value(0).toString());
    methodCombo->setCurrentText(method); methodCombo->setObjectName("verificationMethod");
    auto *levelCombo = new QComboBox; levelCombo->setEditable(true); levelCombo->setInsertPolicy(QComboBox::NoInsert);
    levelCombo->addItem("— Brouillon —", -1);
    for (auto it = ptCodes.cbegin(); it != ptCodes.cend(); ++it) levelCombo->addItem(it.value(), it.key());
    if (verification.levelPtId >= 0 && levelCombo->findData(verification.levelPtId) >= 0)
      levelCombo->setCurrentIndex(levelCombo->findData(verification.levelPtId));
    else if (!verification.level.trimmed().isEmpty()) {
      const int match = levelCombo->findText(verification.level, Qt::MatchFixedString);
      if (match >= 0) levelCombo->setCurrentIndex(match);
      else { levelCombo->insertItem(0, QString::fromUtf8("⚠ ") + verification.level, -2); levelCombo->setCurrentIndex(0); }
    }
    levelCombo->setObjectName("verificationLevel");
    auto *verdictCombo = new QComboBox; verdictCombo->addItems({"", "C", "PC", "NC"});
    if (verdictCombo->findText(verification.verdict) < 0 && !verification.verdict.isEmpty()) verdictCombo->addItem(verification.verdict);
    verdictCombo->setCurrentText(verification.verdict); verdictCombo->setObjectName("verificationVerdict");
    verifications->setCellWidget(r, 0, methodCombo); verifications->setCellWidget(r, 1, levelCombo);
    verifications->setCellWidget(r, 5, verdictCombo);
    const QStringList values{verification.procedure, verification.redmine, verification.means, verification.comment};
    for (int c = 0; c < values.size(); ++c) verifications->setItem(r, c < 3 ? c + 2 : 6, new QTableWidgetItem(values[c]));
    connect(methodCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { if (updateEditorDiagnostics) updateEditorDiagnostics(); });
    connect(levelCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { if (updateEditorDiagnostics) updateEditorDiagnostics(); });
    connect(verdictCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { if (updateEditorDiagnostics) updateEditorDiagnostics(); });
  };
  for (int index = 0; index < item.record.verifications.size(); ++index)
    addVerification(item.record.verifications[index], item.verificationMethods.value(index));
  auto *verificationButtons = new QHBoxLayout;
  auto *addVerificationButton = new QPushButton("Ajouter"); auto *removeVerificationButton = new QPushButton("Supprimer");
  auto *upVerificationButton = new QPushButton("Monter"); auto *downVerificationButton = new QPushButton("Descendre");
  verificationButtons->addWidget(addVerificationButton); verificationButtons->addWidget(removeVerificationButton);
  verificationButtons->addWidget(upVerificationButton); verificationButtons->addWidget(downVerificationButton); verificationButtons->addStretch();
  verificationLayout->addWidget(verifications); verificationLayout->addLayout(verificationButtons);
  connect(addVerificationButton, &QPushButton::clicked, &dialog, [&] { addVerification({}, {}); if (updateEditorDiagnostics) updateEditorDiagnostics(); });
  connect(removeVerificationButton, &QPushButton::clicked, &dialog, [&] { if (verifications->currentRow() >= 0) verifications->removeRow(verifications->currentRow()); if (updateEditorDiagnostics) updateEditorDiagnostics(); });
  auto moveTableRow = [](QTableWidget *table, int from, int to) {
    if (from < 0 || to < 0 || to >= table->rowCount()) return;
    for (int column = 0; column < table->columnCount(); ++column) {
      auto *fromItem = table->takeItem(from, column); auto *toItem = table->takeItem(to, column);
      QWidget *fromWidget = table->cellWidget(from, column); QWidget *toWidget = table->cellWidget(to, column);
      if (fromWidget) table->removeCellWidget(from, column);
      if (toWidget) table->removeCellWidget(to, column);
      table->setItem(from, column, toItem); table->setItem(to, column, fromItem);
      if (toWidget) table->setCellWidget(from, column, toWidget);
      if (fromWidget) table->setCellWidget(to, column, fromWidget);
    }
    table->setCurrentCell(to, 0);
  };
  connect(upVerificationButton, &QPushButton::clicked, &dialog, [&, moveTableRow] { moveTableRow(verifications, verifications->currentRow(), verifications->currentRow() - 1); });
  connect(downVerificationButton, &QPushButton::clicked, &dialog, [&, moveTableRow] { moveTableRow(verifications, verifications->currentRow(), verifications->currentRow() + 1); });
  tabs->addTab(verificationPage, "Vérifications");

  auto *relationPage = new QWidget;
  auto *relationLayout = new QVBoxLayout(relationPage);
  auto *relations = new QTableWidget(0, 4);
  relations->setObjectName("wordRelationTable");
  relations->setHorizontalHeaderLabels({"Type", "Direction (IN/OUT)", "Code lié", "Commentaire"});
  relations->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  auto addRelation = [&](const DocxImportRelation &relation) {
    const int r = relations->rowCount(); relations->insertRow(r);
    auto *typeCombo = new QComboBox; typeCombo->setEditable(true);
    typeCombo->addItem("DECOMPOSE"); typeCombo->addItem("DERIVES_FROM"); typeCombo->addItem("DEPENDS_ON");
    typeCombo->setCurrentText(relation.type); typeCombo->setObjectName("relationType");
    auto *directionCombo = new QComboBox; directionCombo->addItems({"IN", "OUT"});
    directionCombo->setCurrentText(relation.direction); directionCombo->setObjectName("relationDirection");
    auto *requirementCombo = new QComboBox; requirementCombo->setEditable(true); requirementCombo->setInsertPolicy(QComboBox::NoInsert);
    QSet<QString> codes;
    QSqlQuery requirements("SELECT CODE,TITLE FROM REQUIREMENT ORDER BY CODE", editorDb);
    while (requirements.next()) { const QString value=requirements.value(0).toString(); codes.insert(normalized(value)); requirementCombo->addItem(value + " — " + requirements.value(1).toString(), value); }
    for (const auto &candidate : m_wordPreview.requirements)
      if (!candidate.record.code.isEmpty() && !codes.contains(normalized(candidate.record.code)))
        requirementCombo->addItem(candidate.record.code + " — " + candidate.record.title, candidate.record.code);
    int requirementIndex = requirementCombo->findData(relation.otherCode);
    if (requirementIndex >= 0) requirementCombo->setCurrentIndex(requirementIndex); else requirementCombo->setCurrentText(relation.otherCode);
    requirementCombo->setObjectName("relationRequirement");
    relations->setCellWidget(r,0,typeCombo); relations->setCellWidget(r,1,directionCombo); relations->setCellWidget(r,2,requirementCombo);
    relations->setItem(r,3,new QTableWidgetItem(relation.comment));
    connect(typeCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { if (updateEditorDiagnostics) updateEditorDiagnostics(); });
    connect(directionCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { if (updateEditorDiagnostics) updateEditorDiagnostics(); });
    connect(requirementCombo, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { if (updateEditorDiagnostics) updateEditorDiagnostics(); });
  };
  for (const auto &relation : item.relations) addRelation(relation);
  auto *relationButtons = new QHBoxLayout;
  auto *addRelationButton = new QPushButton("Ajouter"); auto *removeRelationButton = new QPushButton("Supprimer");
  auto *upRelationButton = new QPushButton("Monter"); auto *downRelationButton = new QPushButton("Descendre");
  relationButtons->addWidget(addRelationButton); relationButtons->addWidget(removeRelationButton);
  relationButtons->addWidget(upRelationButton); relationButtons->addWidget(downRelationButton); relationButtons->addStretch();
  relationLayout->addWidget(relations); relationLayout->addLayout(relationButtons);
  connect(addRelationButton, &QPushButton::clicked, &dialog, [&] { addRelation({}); if (updateEditorDiagnostics) updateEditorDiagnostics(); });
  connect(removeRelationButton, &QPushButton::clicked, &dialog, [&] { if (relations->currentRow() >= 0) relations->removeRow(relations->currentRow()); if (updateEditorDiagnostics) updateEditorDiagnostics(); });
  connect(upRelationButton, &QPushButton::clicked, &dialog, [&, moveTableRow] { moveTableRow(relations, relations->currentRow(), relations->currentRow() - 1); });
  connect(downRelationButton, &QPushButton::clicked, &dialog, [&, moveTableRow] { moveTableRow(relations, relations->currentRow(), relations->currentRow() + 1); });
  tabs->addTab(relationPage, "Relations");

  auto *diagnosticSummary = new QLabel;
  diagnosticSummary->setObjectName("wordCorrectionDiagnostics");
  diagnosticSummary->setWordWrap(true);
  auto comboKnown = [](QComboBox *combo) {
    return combo->currentText().trimmed().isEmpty() || combo->findText(combo->currentText(), Qt::MatchFixedString) >= 0;
  };
  updateEditorDiagnostics = [&, comboKnown] {
    QStringList generalErrors, verificationErrors, relationErrors;
    auto mark = [](QWidget *widget, const QString &message) {
      if (!widget) return;
      widget->setStyleSheet(message.isEmpty() ? QString() : "background:#fde7e9;border:1px solid #b42318;");
      widget->setToolTip(message);
    };
    mark(code, {}); mark(title, {}); mark(description, {}); mark(type, {}); mark(status, {});
    if (code->text().trimmed().isEmpty()) { generalErrors << "Code absent"; mark(code, generalErrors.last()); }
    if (title->text().trimmed().isEmpty()) { generalErrors << "Titre absent"; mark(title, generalErrors.last()); }
    if (description->toPlainText().trimmed().isEmpty()) { generalErrors << "Description absente"; mark(description, generalErrors.last()); }
    if (!comboKnown(type)) { generalErrors << "Type inconnu : " + type->currentText(); mark(type, generalErrors.last()); }
    if (!comboKnown(status)) { generalErrors << "Statut inconnu : " + status->currentText(); mark(status, generalErrors.last()); }
    for (int r = 0; r < verifications->rowCount(); ++r)
      for (int c = 0; c < verifications->columnCount(); ++c) {
        if (auto *widget = verifications->cellWidget(r,c)) mark(widget, {});
        if (auto *cell = verifications->item(r,c)) { cell->setBackground(QBrush()); cell->setToolTip({}); }
      }
    QSet<QString> keys;
    for (int r = 0; r < verifications->rowCount(); ++r) {
      auto *methodCombo = qobject_cast<QComboBox *>(verifications->cellWidget(r,0));
      auto *levelCombo = qobject_cast<QComboBox *>(verifications->cellWidget(r,1));
      auto *verdictCombo = qobject_cast<QComboBox *>(verifications->cellWidget(r,5));
      QString error;
      if (!methodCombo || methodCombo->currentText().trimmed().isEmpty()) error = "Méthode absente";
      else if (!comboKnown(methodCombo)) error = "Méthode inconnue : " + methodCombo->currentText();
      if (!error.isEmpty()) { verificationErrors << QString("Ligne %1 : %2").arg(r+1).arg(error); mark(methodCombo,error); }
      if (levelCombo && levelCombo->currentData().toInt() == -2) {
        error = "Niveau PT inconnu : " + levelCombo->currentText().mid(2);
        verificationErrors << QString("Ligne %1 : %2").arg(r+1).arg(error); mark(levelCombo,error);
      }
      if (methodCombo && !methodCombo->currentText().trimmed().isEmpty() && comboKnown(methodCombo) && levelCombo && levelCombo->currentData().toInt() != -2) {
        const QString key = normalized(methodCombo->currentText()) + ":" + QString::number(levelCombo->currentData().toInt());
        if (keys.contains(key)) {
          error = levelCombo->currentData().toInt() < 0 ? "Un seul brouillon est autorisé pour cette méthode."
                                                        : "Cette méthode existe déjà à ce niveau PT.";
          verificationErrors << QString("Ligne %1 : %2").arg(r+1).arg(error); mark(methodCombo,error); mark(levelCombo,error);
        }
        keys.insert(key);
      }
      if (verdictCombo && !QStringList{"", "C", "PC", "NC"}.contains(verdictCombo->currentText())) {
        error = "Verdict invalide"; verificationErrors << QString("Ligne %1 : %2").arg(r+1).arg(error); mark(verdictCombo,error);
      }
    }
    for (int r = 0; r < relations->rowCount(); ++r) {
      for (int c=0;c<3;++c) mark(relations->cellWidget(r,c),{});
      auto *relationType = qobject_cast<QComboBox *>(relations->cellWidget(r,0));
      auto *direction = qobject_cast<QComboBox *>(relations->cellWidget(r,1));
      auto *other = qobject_cast<QComboBox *>(relations->cellWidget(r,2));
      if (!relationType || DocxImportService::normalizedRelationTypeCode(relationType->currentText()).isEmpty()) {
        const QString error="Type de relation inconnu"; relationErrors << QString("Ligne %1 : %2").arg(r+1).arg(error); mark(relationType,error);
      }
      if (!direction || !QStringList{"IN","OUT"}.contains(direction->currentText())) {
        const QString error="Direction invalide"; relationErrors << QString("Ligne %1 : %2").arg(r+1).arg(error); mark(direction,error);
      }
      QString otherCode = other ? other->currentData().toString() : QString();
      if (otherCode.isEmpty() && other) otherCode = other->currentText().section(" — ",0,0).trimmed();
      QSet<QString> availableCodes;
      QSqlQuery linkedRequirements("SELECT CODE FROM REQUIREMENT", editorDb);
      while (linkedRequirements.next()) availableCodes.insert(normalized(linkedRequirements.value(0).toString()));
      for (const auto &candidate : m_wordPreview.requirements) availableCodes.insert(normalized(candidate.record.code));
      if (otherCode.isEmpty() || !availableCodes.contains(normalized(otherCode))) {
        const QString error=otherCode.isEmpty() ? "Exigence liée absente" : "Exigence liée inconnue : " + otherCode;
        relationErrors << QString("Ligne %1 : %2").arg(r+1).arg(error); mark(other,error);
      }
    }
    tabs->setTabText(0, generalErrors.isEmpty() ? "Exigence" : QString("Exigence (%1)").arg(generalErrors.size()));
    tabs->setTabText(1, verificationErrors.isEmpty() ? "Vérifications" : QString("Vérifications (%1)").arg(verificationErrors.size()));
    tabs->setTabText(2, relationErrors.isEmpty() ? "Relations" : QString("Relations (%1)").arg(relationErrors.size()));
    QStringList all = generalErrors + verificationErrors + relationErrors;
    diagnosticSummary->setText(all.isEmpty() ? QString::fromUtf8("✓ Aucune anomalie détectée") : QString::fromUtf8("⚠ ") + all.join("  •  "));
    diagnosticSummary->setStyleSheet(all.isEmpty() ? "QLabel{color:#067647;background:#e8f5e9;padding:6px;}"
                                                   : "QLabel{color:#b42318;background:#fde7e9;padding:6px;}");
  };
  connect(code, &QLineEdit::textChanged, &dialog, [&](const QString &) { updateEditorDiagnostics(); });
  connect(title, &QLineEdit::textChanged, &dialog, [&](const QString &) { updateEditorDiagnostics(); });
  connect(type, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { updateEditorDiagnostics(); });
  connect(status, &QComboBox::currentTextChanged, &dialog, [&](const QString &) { updateEditorDiagnostics(); });
  connect(description, &QTextEdit::textChanged, &dialog, [&] { updateEditorDiagnostics(); });
  updateEditorDiagnostics();
  for (const auto &detail : item.detailedDiagnostics) {
    if (!detail.blocking) continue;
    if (detail.tab == "Vérifications") tabs->setCurrentIndex(1);
    else if (detail.tab == "Relations") tabs->setCurrentIndex(2);
    else tabs->setCurrentIndex(0);
    break;
  }

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  auto *layout = new QVBoxLayout(&dialog); layout->addWidget(diagnosticSummary); layout->addWidget(tabs); layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() != QDialog::Accepted) return;

  item.record.code = code->text().trimmed(); item.record.title = title->text().trimmed();
  item.record.source = source->text().trimmed(); item.type = type->currentText().trimmed(); item.status = status->currentText().trimmed();
  item.record.traceRoot = traceRoot->isChecked();
  item.chapterPath = editedLines(chapter->text().replace('/', '\n'));
  item.productTreeCodes.clear();
  for (const int id : pts->checkedIds()) item.productTreeCodes << ptCodes.value(id);
  item.configurationCodes.clear();
  for (const int id : configs->checkedIds()) item.configurationCodes << configurationCodes.value(id);
  const QString editedComments = comments->toPlainText();
  item.commentsProvided = item.commentsProvided || editedComments != originalComments;
  item.record.comments = editedComments;
  item.record.description = description->document()->toMarkdown(
      QTextDocument::MarkdownDialectGitHub).trimmed();
  item.record.verifications.clear(); item.verificationMethods.clear();
  for (int r = 0; r < verifications->rowCount(); ++r) {
    auto *methodCombo=qobject_cast<QComboBox *>(verifications->cellWidget(r,0));
    auto *levelCombo=qobject_cast<QComboBox *>(verifications->cellWidget(r,1));
    auto *verdictCombo=qobject_cast<QComboBox *>(verifications->cellWidget(r,5));
    RequirementVerification verification; item.verificationMethods << (methodCombo ? methodCombo->currentText().trimmed() : QString());
    if (levelCombo && levelCombo->currentData().toInt() >= 0) verification.levelPtId=levelCombo->currentData().toInt();
    else if (levelCombo && levelCombo->currentData().toInt() == -2) verification.level=levelCombo->currentText().mid(2).trimmed();
    verification.procedure = verifications->item(r, 2)->text().trimmed();
    verification.redmine = verifications->item(r, 3)->text().trimmed(); verification.means = verifications->item(r, 4)->text().trimmed();
    verification.verdict = verdictCombo ? verdictCombo->currentText().trimmed() : QString(); verification.comment = verifications->item(r, 6)->text().trimmed();
    item.record.verifications << verification;
  }
  item.relations.clear();
  for (int r = 0; r < relations->rowCount(); ++r) {
    auto *typeCombo=qobject_cast<QComboBox *>(relations->cellWidget(r,0));
    auto *directionCombo=qobject_cast<QComboBox *>(relations->cellWidget(r,1));
    auto *requirementCombo=qobject_cast<QComboBox *>(relations->cellWidget(r,2));
    QString otherCode=requirementCombo ? requirementCombo->currentData().toString() : QString();
    if (otherCode.isEmpty() && requirementCombo) otherCode=requirementCombo->currentText().section(" — ",0,0).trimmed();
    item.relations << DocxImportRelation{typeCombo ? typeCombo->currentText().trimmed() : QString(),
                                        directionCombo ? directionCombo->currentText().trimmed().toUpper() : QString(),
                                        otherCode, relations->item(r, 3)->text().trimmed()};
  }
  item.recognition = DocxImportRecognition::Conformant;
  revalidateWordPreview();
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
      {"NIVEAU 2", "LEVEL 2"}, {"SECTION", "CHAPTER", "CHAPITRE"},
      {"RACINE DE TRACABILITE", "RACINE DE TRAÇABILITÉ", "TRACE ROOT",
       "ROOT", "IS_TRACE_ROOT"},
      {"COMMENTAIRES", "COMMENTS", "COMMENT"}};
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
    existing.prepare("SELECT TITLE,COALESCE(DESCRIPTION,'') FROM REQUIREMENT WHERE CODE=? COLLATE NOCASE");
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
    existing.prepare("SELECT ID FROM REQUIREMENT WHERE CODE=? COLLATE NOCASE");
    existing.addBindValue(code);
    const bool exists = existing.exec() && existing.next();
    if (codeWasEmpty || (!exists && (title.isEmpty() || description.isEmpty()))) {
      errors << QString("Ligne %1 : code absent ou titre/descriptif absent pour une création.")
                    .arg(index + 1);
      continue;
    }
    bool importedTraceRoot = false;
    const bool traceRootIsExplicit =
        m_traceRootOverride >= 0 || mappedColumn(TraceRoot) >= 0;
    if (m_traceRootOverride >= 0) {
      importedTraceRoot = m_traceRootOverride == 1;
    } else if (mappedColumn(TraceRoot) >= 0 &&
               !parseTraceRootValue(mappedValue(row, TraceRoot),
                                    &importedTraceRoot)) {
      errors << QString("Ligne %1 : valeur de racine de traçabilité invalide « %2 ».")
                    .arg(index + 1)
                    .arg(mappedValue(row, TraceRoot));
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
      if (traceRootIsExplicit)
        record.traceRoot = importedTraceRoot;
      if (mappedColumn(Comments) >= 0)
        record.comments = mappedValue(row, Comments);
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
    document.prepare("SELECT COALESCE((SELECT REFERENCE FROM DOCUMENT_REFERENCE WHERE DOC_ID=? AND IS_PRIMARY=1 ORDER BY ID LIMIT 1),'')");
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
    document.prepare("INSERT INTO DOCUMENT(PT_ID,TYPE,TITLE,DESCRIPTION) VALUES(?,COALESCE((SELECT ID FROM DOC_TYPE WHERE UPPER(TYPE)='SP' LIMIT 1),(SELECT ID FROM DOC_TYPE ORDER BY ID LIMIT 1)),?,?)");
    document.addBindValue(rootId);
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
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  for (int row = 0; row < selected.requirements.size(); ++row) {
    const bool checked = m_preview->item(row, WordSelected) && m_preview->item(row, WordSelected)->checkState() == Qt::Checked;
    selected.requirements[row].selected = checked;
    m_wordPreview.requirements[row].selected = checked;
    const bool traceRoot = m_preview->item(row, WordTraceRoot) &&
                           m_preview->item(row, WordTraceRoot)->checkState() ==
                               Qt::Checked;
    selected.requirements[row].record.traceRoot = traceRoot;
    m_wordPreview.requirements[row].record.traceRoot = traceRoot;
  }
  DocxImportService validator(m_connection);
  if (!validator.validatePreview(&selected)) {
    int firstInvalid = -1;
    for (int row = 0; row < selected.requirements.size(); ++row) {
      m_wordPreview.requirements[row].diagnostics = selected.requirements[row].diagnostics;
      m_wordPreview.requirements[row].detailedDiagnostics = selected.requirements[row].detailedDiagnostics;
      refreshWordPreviewRow(row);
      if (firstInvalid < 0 && selected.requirements[row].selected)
        for (const auto &detail : selected.requirements[row].detailedDiagnostics)
          if (detail.blocking) { firstInvalid = row; break; }
    }
    updateWordImportAvailability();
    if (firstInvalid >= 0) {
      m_preview->selectRow(firstInvalid);
      QMessageBox::warning(this, "Import Word",
                           "Des anomalies bloquantes sont signalées dans l’aperçu. Corrigez l’exigence marquée ou décochez-la avant l’import.");
    }
    return;
  }
  ProductTreeService productTrees(m_connection);
  auto lookup = [&](const QString &table, const QStringList &columns, const QString &value) {
    if (value.trimmed().isEmpty()) return true;
    for (const QString &column : columns) {
      QSqlQuery query(db);
      query.prepare(QString("SELECT 1 FROM %1 WHERE UPPER(TRIM(%2))=? LIMIT 1").arg(table, column));
      query.addBindValue(normalized(value));
      if (query.exec() && query.next()) return true;
    }
    return false;
  };
  auto requirementExists = [&](const QString &code) {
    for (const auto &candidate : selected.requirements)
      if (candidate.selected && normalized(candidate.record.code) == normalized(code)) return true;
    QSqlQuery query(db); query.prepare("SELECT 1 FROM REQUIREMENT WHERE CODE=? COLLATE NOCASE LIMIT 1");
    query.addBindValue(normalized(code)); return query.exec() && query.next();
  };
  QList<int> invalidRows;
  for (int row = 0; row < selected.requirements.size(); ++row) {
    auto &item = selected.requirements[row];
    if (!item.selected) continue;
    item.diagnostics.clear();
    if (item.record.code.trimmed().isEmpty()) item.diagnostics << "Code absent";
    if (item.record.title.trimmed().isEmpty()) item.diagnostics << "Titre absent";
    if (MarkdownUtility::toPlainText(item.record.description).trimmed().isEmpty()) item.diagnostics << "Description absente";
    if (!item.type.isEmpty() && !lookup("REQ_TYPE", {"TYPE", "CODE"}, item.type)) item.diagnostics << "Type inconnu : " + item.type;
    if (!item.status.isEmpty() && !lookup("REQ_STATUS", {"STATUS", "SHORTCUT"}, item.status)) item.diagnostics << "Statut inconnu : " + item.status;
    for (const QString &code : item.productTreeCodes) {
      bool found = lookup("PT", {"SEGMENT", "NAME"}, code);
      if (!found) { QSqlQuery all("SELECT ID FROM PT WHERE ARCHIVED=0", db); while (all.next()) if (normalized(productTrees.fullCode(all.value(0).toInt())) == normalized(code)) { found = true; break; } }
      if (!found) item.diagnostics << "Product Tree inconnu : " + code;
    }
    for (const QString &code : item.configurationCodes)
      if (!lookup("CONFIGURATION", {"CODE", "LABEL"}, code)) item.diagnostics << "Configuration inconnue : " + code;
    for (const QString &method : item.verificationMethods) {
      if (method.trimmed().isEmpty()) item.diagnostics << "Méthode de vérification absente";
      else if (!lookup("REQ_METHOD", {"METHOD"}, method)) item.diagnostics << "Méthode inconnue : " + method;
    }
    for (const auto &relation : item.relations) {
      if (DocxImportService::normalizedRelationTypeCode(relation.type).isEmpty())
        item.diagnostics << "Type de relation inconnu : " + relation.type;
      if (relation.direction != "IN" && relation.direction != "OUT") item.diagnostics << "Direction de relation invalide : " + relation.direction;
      if (!requirementExists(relation.otherCode)) item.diagnostics << "Exigence liée inconnue : " + relation.otherCode;
    }
    m_wordPreview.requirements[row].diagnostics = item.diagnostics;
    refreshWordPreviewRow(row);
    if (!item.diagnostics.isEmpty()) invalidRows << row;
  }
  if (!invalidRows.isEmpty()) {
    QMessageBox choice(QMessageBox::Warning, "Exigences à corriger",
                       QString("%1 exigence(s) sélectionnée(s) présentent des anomalies.\nAucune modification n'a encore été effectuée.").arg(invalidRows.size()),
                       QMessageBox::NoButton, this);
    auto *correct = choice.addButton("Corriger", QMessageBox::AcceptRole);
    auto *ignore = choice.addButton("Ignorer ces exigences", QMessageBox::DestructiveRole);
    auto *cancelAll = choice.addButton("Annuler tout l'import", QMessageBox::RejectRole);
    choice.setDefaultButton(correct); choice.exec();
    if (choice.clickedButton() == correct) { m_preview->selectRow(invalidRows.first()); editWordRequirement(invalidRows.first()); return; }
    if (choice.clickedButton() == cancelAll) return;
    if (choice.clickedButton() == ignore)
      for (const int row : invalidRows) {
        selected.requirements[row].selected = false; m_wordPreview.requirements[row].selected = false;
        m_preview->item(row, WordSelected)->setCheckState(Qt::Unchecked);
      }
  }
  int selectedCount = 0;
  for (const auto &item : selected.requirements) if (item.selected) ++selectedCount;
  if (selectedCount == 0) { QMessageBox::information(this, "Import Word", "Aucune exigence n'est sélectionnée."); return; }
  const int ignoredCount = selected.requirements.size() - selectedCount;
  if (QMessageBox::question(this, "Confirmer l'import Word",
                            QString("%1 exigence(s) seront importées et %2 ignorée(s).\nConfirmer l'écriture dans la spécification ?")
                                .arg(selectedCount).arg(ignoredCount)) != QMessageBox::Yes) return;
  for (int index = selected.requirements.size() - 1; index >= 0; --index)
    if (!selected.requirements[index].selected) selected.requirements.removeAt(index);

  QList<ImportDuplicate> duplicateRows;
  for (const auto &item : selected.requirements) {
    QSqlQuery existing(db); existing.prepare("SELECT TITLE,COALESCE(DESCRIPTION,'') FROM REQUIREMENT WHERE CODE=? COLLATE NOCASE"); existing.addBindValue(item.record.code);
    if (existing.exec() && existing.next()) duplicateRows << ImportDuplicate{item.record.code, existing.value(0).toString(), item.record.title,
      MarkdownUtility::toPlainText(MarkdownUtility::normalizeLegacy(existing.value(1).toString())),
      MarkdownUtility::toPlainText(MarkdownUtility::normalizeLegacy(item.record.description))};
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
  if (!result.success) {
    QMessageBox::critical(this, "Import Word", result.message +
                          "\n\nL'import a été entièrement annulé : aucune modification n'a été conservée.\n"
                          "Les corrections de l'aperçu sont conservées pour une nouvelle tentative.");
    return;
  }
  QMessageBox::information(this, "Import Word", result.message); emit imported(); accept();
}
