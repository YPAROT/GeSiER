#include "requirementwidget.h"
#include "checkablecombobox.h"
#include "requirementimportdialog.h"
#include "requirementrelationservice.h"
#include "tabularservice.h"
#include "tabularexportdialog.h"
#include "documentservice.h"
#include "changeservice.h"
#include "historyservice.h"
#include "producttreeservice.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QtWidgets>
#include <algorithm>
#include <limits>

namespace {
struct RelationChoice { int id = -1; QString comment; };

RelationChoice chooseRequirement(QWidget *parent, QSqlDatabase db, int excluded,
                                 const QString &caption) {
  QDialog dialog(parent);
  dialog.setWindowTitle(caption);
  dialog.resize(850, 480);
  auto *layout = new QVBoxLayout(&dialog);
  auto *search = new QLineEdit;
  search->setPlaceholderText("Rechercher par code ou titre…");
  auto *table = new QTableWidget(0, 5);
  table->setHorizontalHeaderLabels({"Code", "Titre", "Statut", "Type", "Allocations PT"});
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->horizontalHeader()->setStretchLastSection(true);
  auto *comment = new QLineEdit;
  comment->setPlaceholderText("Commentaire de la relation (facultatif)");
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
  layout->addWidget(search); layout->addWidget(table); layout->addWidget(comment); layout->addWidget(buttons);
  auto reload = [=] {
    table->setRowCount(0);
    QSqlQuery query(db);
    query.prepare("SELECT R.ID,R.CODE,R.TITLE,COALESCE(S.STATUS,''),COALESCE(T.TYPE,''),COALESCE((SELECT GROUP_CONCAT(P.NAME,', ') FROM REQUIREMENT_PT RP JOIN PT P ON P.ID=RP.PT_ID WHERE RP.REQ_ID=R.ID),'') FROM REQUIREMENT R LEFT JOIN REQ_STATUS S ON S.ID=R.STATUS LEFT JOIN REQ_TYPE T ON T.ID=R.TYPE WHERE R.ID<>? AND (R.CODE LIKE ? OR R.TITLE LIKE ?) ORDER BY R.CODE LIMIT 200");
    const QString pattern = "%" + search->text().trimmed() + "%";
    query.addBindValue(excluded); query.addBindValue(pattern); query.addBindValue(pattern);
    if (!query.exec()) return;
    while (query.next()) {
      const int row = table->rowCount(); table->insertRow(row);
      for (int column = 0; column < 5; ++column)
        table->setItem(row, column, new QTableWidgetItem(query.value(column + 1).toString()));
      table->item(row, 0)->setData(Qt::UserRole, query.value(0));
    }
  };
  QObject::connect(search, &QLineEdit::textChanged, &dialog, reload);
  QObject::connect(table, &QTableWidget::itemSelectionChanged, &dialog, [=] { buttons->button(QDialogButtonBox::Ok)->setEnabled(table->currentRow() >= 0); });
  QObject::connect(table, &QTableWidget::cellDoubleClicked, &dialog, [&](int, int) { dialog.accept(); });
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  reload(); search->setFocus();
  if (dialog.exec() != QDialog::Accepted || table->currentRow() < 0) return {};
  return {table->item(table->currentRow(), 0)->data(Qt::UserRole).toInt(), comment->text()};
}
}

enum Column {
  CodeCol,
  TitleCol,
  StatusCol,
  TypeCol,
  PtCol,
  MethodCol,
  DescriptionCol,
  ApplicabilityCol,
  SourceCol,
  AllocatedCol,
  TracedCol,
  DocumentedCol,
  VerifiedCol,
  ColumnCount
};

RequirementWidget::RequirementWidget(QWidget *p) : QWidget(p) {
  auto add = new QPushButton("Nouvelle");
  add->setObjectName("newRequirementButton");
  m_duplicate = new QPushButton("Dupliquer");
  m_obsoleteButton = new QPushButton("Rendre obsolète");
  m_includeObsolete = new QCheckBox("Inclure les obsolètes");
  auto *importExcel = new QPushButton("Importer XLSX…");
  importExcel->setText("Importer CSV/XLSX…");
  auto *exportTable = new QPushButton("Exporter CSV/XLSX…");
  auto *changes = new QPushButton("Changements liés");
  auto toolbar = new QHBoxLayout;
  toolbar->addWidget(add);
  toolbar->addWidget(m_duplicate);
  toolbar->addWidget(m_obsoleteButton);
  toolbar->addWidget(importExcel);
  toolbar->addWidget(exportTable);
  toolbar->addWidget(changes);
  toolbar->addStretch();
  toolbar->addWidget(m_includeObsolete);
  connect(changes, &QPushButton::clicked, this, [this] {
    if (m_current >= 0)
      emit openChangesRequested(m_current);
  });
  m_list = new QTableWidget(1, ColumnCount);
  m_list->setObjectName("requirementList");
  m_list->setHorizontalHeaderLabels(
      {"Code", "Titre", "Statut", "Type", "Product Trees", "Méthodes",
       "Description", "Applicabilité", "Source", "Allocation PT",
       "Traçabilité", "Documentation", "Vérification"});
  m_list->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_list->setSelectionMode(QAbstractItemView::SingleSelection);
  m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_list->verticalHeader()->hide();
  m_list->horizontalHeader()->setSectionsMovable(true);
  m_list->horizontalHeader()->setSortIndicatorShown(true);
  m_codeFilter = new QLineEdit;
  m_titleFilter = new QLineEdit;
  m_descriptionFilter = new QLineEdit;
  m_sourceFilter = new QLineEdit;
  m_statusFilter = new CheckableComboBox("Statuts");
  m_statusFilter->setObjectName("statusFilter");
  m_typeFilter = new CheckableComboBox("Types");
  m_ptFilter = new CheckableComboBox("PT");
  m_methodFilter = new CheckableComboBox("Méthodes");
  m_applicabilityFilter = new CheckableComboBox("Configurations");
  m_allocatedFilter = new CheckableComboBox("Allocation PT");
  m_allocatedFilter->setObjectName("allocatedFilter");
  m_tracedFilter = new CheckableComboBox("Traçabilité");
  m_documentedFilter = new CheckableComboBox("Documentation");
  m_verifiedFilter = new CheckableComboBox("Vérification");
  for (CheckableComboBox *filter : {m_allocatedFilter, m_tracedFilter,
                                    m_documentedFilter, m_verifiedFilter}) {
    filter->addItem("Couverte", 1);
    filter->addItem("Non couverte", 0);
  }
  QList<QWidget *> filters = {m_codeFilter,        m_titleFilter,
                              m_statusFilter,      m_typeFilter,
                              m_ptFilter,          m_methodFilter,
                              m_descriptionFilter, m_applicabilityFilter,
                              m_sourceFilter,      m_allocatedFilter,
                              m_tracedFilter,      m_documentedFilter,
                              m_verifiedFilter};
  for (int c = 0; c < filters.size(); ++c)
    m_list->setCellWidget(0, c, filters[c]);
  m_list->setRowHeight(0, 30);
  m_tabs = new QTabWidget;
  m_code = new QLineEdit;
  m_codeWarning = new QLabel;
  m_codeWarning->setStyleSheet("color:#b06000");
  m_codeWarning->setWordWrap(true);
  m_title = new QLineEdit;
  m_description = new QTextEdit;
  m_source = new QLineEdit;
  m_status = new QComboBox;
  m_type = new QComboBox;
  auto identity = new QWidget;
  auto identityForm = new QFormLayout(identity);
  identityForm->addRow("Code", m_code);
  identityForm->addRow(QString(), m_codeWarning);
  identityForm->addRow("Titre", m_title);
  identityForm->addRow("Description", m_description);
  identityForm->addRow("Source", m_source);
  identityForm->addRow("Type", m_type);
  identityForm->addRow("Statut", m_status);
  m_tabs->addTab(identity, "Identité");
  m_primary = new QComboBox;
  m_primary->setObjectName("primaryProductTree");
  m_pt = new QTreeWidget;
  m_pt->setObjectName("allocationProductTree");
  m_pt->setHeaderLabels({"Allocations Product Tree"});
  auto ptPage = new QWidget;
  auto ptLayout = new QVBoxLayout(ptPage);
  auto primaryForm = new QFormLayout;
  primaryForm->addRow("PT principal", m_primary);
  ptLayout->addLayout(primaryForm);
  ptLayout->addWidget(m_pt);
  m_tabs->addTab(ptPage, "Product Tree");
  m_verifications = new QTableWidget(0, 7);
  m_verifications->setHorizontalHeaderLabels(
      {"Méthode", "Niveau", "Procédure / test", "Redmine", "Moyen", "Verdict",
       "Commentaire"});
  m_verifications->horizontalHeader()->setStretchLastSection(true);
  m_verifications->setSelectionBehavior(QAbstractItemView::SelectRows);
  auto addVerif = new QPushButton("Ajouter");
  auto removeVerif = new QPushButton("Supprimer");
  auto up = new QPushButton("Monter");
  auto down = new QPushButton("Descendre");
  auto verifButtons = new QHBoxLayout;
  verifButtons->addWidget(addVerif);
  verifButtons->addWidget(removeVerif);
  verifButtons->addWidget(up);
  verifButtons->addWidget(down);
  verifButtons->addStretch();
  auto verificationPage = new QWidget;
  auto verificationLayout = new QVBoxLayout(verificationPage);
  verificationLayout->addLayout(verifButtons);
  verificationLayout->addWidget(m_verifications);
  m_tabs->addTab(verificationPage, "Vérification");
  m_relations = new QTableWidget(0, 4);
  m_relations->setHorizontalHeaderLabels({"Relation", "Code", "Titre", "Commentaire"});
  m_relations->setSelectionBehavior(QAbstractItemView::SelectRows);
  auto *addUpstream = new QPushButton("Ajouter une relation amont");
  auto *addDownstream = new QPushButton("Ajouter un enfant / aval");
  auto *removeRelation = new QPushButton("Supprimer le lien");
  auto *relationActions = new QHBoxLayout;
  relationActions->addWidget(addUpstream); relationActions->addWidget(addDownstream);
  relationActions->addWidget(removeRelation); relationActions->addStretch();
  auto *relationPage = new QWidget;
  auto *relationLayout = new QVBoxLayout(relationPage);
  relationLayout->addLayout(relationActions); relationLayout->addWidget(m_relations);
  m_tabs->addTab(relationPage, "Relations");
  auto *graphPage = new QWidget;
  auto *graphLayout = new QVBoxLayout(graphPage);
  auto *graphControls = new QHBoxLayout;
  graphControls->addWidget(new QLabel("Profondeur"));
  m_graphDepth = new QSpinBox;
  m_graphDepth->setObjectName("relationGraphDepth");
  m_graphDepth->setRange(1, 8); m_graphDepth->setValue(2);
  m_showDerivations = new QCheckBox("Afficher les dérivations");
  m_showDerivations->setObjectName("showRelationDerivations");
  m_showDependencies = new QCheckBox("Afficher les dépendances");
  m_showDependencies->setObjectName("showRelationDependencies");
  QSettings graphSettings;
  m_showDerivations->setChecked(
      graphSettings.value("Requirements/graph/showDerivations", true).toBool());
  m_showDependencies->setChecked(
      graphSettings.value("Requirements/graph/showDependencies", true).toBool());
  graphControls->addWidget(m_graphDepth);
  graphControls->addSpacing(12);
  graphControls->addWidget(m_showDerivations);
  graphControls->addWidget(m_showDependencies);
  graphControls->addStretch();
  m_relationGraph = new QGraphicsView;
  m_relationGraph->setRenderHint(QPainter::Antialiasing);
  graphLayout->addLayout(graphControls); graphLayout->addWidget(m_relationGraph);
  m_tabs->addTab(graphPage, "Graphe");

  m_documents = new QTableWidget(0, 3);
  m_documents->setHorizontalHeaderLabels({"Document", "Référence", "Emplacement"});
  m_documents->setSelectionBehavior(QAbstractItemView::SelectRows);
  auto *addToDocument = new QPushButton("Ajouter / déplacer");
  auto *removeFromDocument = new QPushButton("Retirer du document");
  auto *documentActions = new QHBoxLayout;
  documentActions->addWidget(addToDocument); documentActions->addWidget(removeFromDocument);
  documentActions->addStretch();
  auto *documentsPage = new QWidget;
  auto *documentsLayout = new QVBoxLayout(documentsPage);
  documentsLayout->addLayout(documentActions); documentsLayout->addWidget(m_documents);
  m_tabs->addTab(documentsPage, "Documents");

  m_configurations = new QListWidget;
  auto *applicabilityPage = new QWidget;
  auto *applicabilityLayout = new QVBoxLayout(applicabilityPage);
  auto *applicabilityHelp = new QLabel(
      "Cochez les configurations auxquelles l'exigence s'applique. "
      "Les configurations se gèrent dans Projet > Applicabilité.");
  applicabilityHelp->setWordWrap(true);
  applicabilityLayout->addWidget(applicabilityHelp);
  applicabilityLayout->addWidget(m_configurations);
  m_tabs->addTab(applicabilityPage, "Applicabilité");
  m_changes = new QTableWidget(0, 5);
  m_changes->setHorizontalHeaderLabels(
      {"Code", "Type", "Statut", "Description", "Décision"});
  m_changes->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_changes->setSelectionMode(QAbstractItemView::SingleSelection);
  m_changes->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_changes->horizontalHeader()->setStretchLastSection(true);
  auto *changesPage = new QWidget;
  auto *changesLayout = new QVBoxLayout(changesPage);
  auto *openAllChanges = new QPushButton("Ouvrir les changements liés");
  changesLayout->addWidget(openAllChanges, 0, Qt::AlignLeft);
  changesLayout->addWidget(m_changes);
  m_tabs->addTab(changesPage, "Changements");

  m_history = new QTableWidget(0, 6);
  m_history->setHorizontalHeaderLabels(
      {"Date", "Auteur", "Événement", "Avant", "Après", "Commentaire"});
  m_history->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_history->horizontalHeader()->setStretchLastSection(true);
  m_tabs->addTab(m_history, "Historique");
  connect(openAllChanges, &QPushButton::clicked, this, [this] {
    if (m_current >= 0) emit openChangesRequested(m_current);
  });
  connect(m_changes, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    if (row >= 0 && m_changes->item(row, 0))
      emit openChangeRequested(
          m_changes->item(row, 0)->data(Qt::UserRole).toInt());
  });
  m_save = new QPushButton("Enregistrer");
  m_cancel = new QPushButton("Annuler");
  auto actions = new QHBoxLayout;
  actions->addStretch();
  actions->addWidget(m_cancel);
  actions->addWidget(m_save);
  m_editor = new QWidget;
  m_editor->setObjectName("requirementEditor");
  auto editorLayout = new QVBoxLayout(m_editor);
  editorLayout->addWidget(m_tabs);
  editorLayout->addLayout(actions);
  m_splitter = new QSplitter;
  m_splitter->addWidget(m_list);
  m_splitter->addWidget(m_editor);
  m_splitter->setStretchFactor(0, 3);
  m_splitter->setStretchFactor(1, 2);
  m_splitter->restoreState(
      QSettings().value("Requirements/splitter").toByteArray());
  auto root = new QVBoxLayout(this);
  root->addLayout(toolbar);
  root->addWidget(m_splitter);
  connect(add, &QPushButton::clicked, this, &RequirementWidget::create);
  connect(importExcel, &QPushButton::clicked, this, [this] {
    if (!QSqlDatabase::database(m_connection).isOpen())
      return;
    RequirementImportDialog dialog(m_connection, this);
    connect(&dialog, &RequirementImportDialog::imported, this, [this] {
      refresh();
      emit dataChanged();
    });
    dialog.exec();
  });
  connect(exportTable, &QPushButton::clicked, this, [this] {
    QStringList availableHeaders;
    for (int column = 0; column < m_list->columnCount(); ++column)
      availableHeaders << m_list->horizontalHeaderItem(column)->text();
    TabularExportDialog options("requirements-export", availableHeaders, this);
    if (options.exec() != QDialog::Accepted) return;
    QString path = QFileDialog::getSaveFileName(
        this, "Exporter les exigences filtrées", "exigences.xlsx",
        "Classeur Excel (*.xlsx);;CSV (*.csv)");
    if (path.isEmpty()) return;
    const bool csv = path.endsWith(".csv", Qt::CaseInsensitive);
    if (!csv && !path.endsWith(".xlsx", Qt::CaseInsensitive)) path += ".xlsx";
    const QList<int> columns = options.columns();
    QList<QStringList> rows;
    rows << options.outputHeaders();
    for (int row = 1; row < m_list->rowCount(); ++row) {
      QStringList values;
      for (int column : columns)
        values << (m_list->item(row, column) ? m_list->item(row, column)->text()
                                             : QString());
      rows << values;
    }
    QString error;
    const bool ok = csv ? TabularService::writeCsv(path, {options.sheetName(), rows}, ';', &error)
                        : TabularService::writeXlsx(path, {{options.sheetName(), rows}}, &error);
    if (ok) QMessageBox::information(this, "Export tabulaire",
                                     QString("%1 exigence(s) exportée(s).").arg(rows.size() - 1));
    else QMessageBox::warning(this, "Export tabulaire", error);
  });
  connect(m_duplicate, &QPushButton::clicked, this, [this] {
    if (m_current < 0)
      return;
    RequirementRecord source = editorRecord();
    create();
    m_loading = true;
    m_title->setText(source.title + " (copie)");
    m_description->setPlainText(source.description);
    m_source->setText(source.source);
    m_type->setCurrentIndex(m_type->findData(source.typeId));
    m_status->setCurrentIndex(m_status->findData(source.statusId));
    QTreeWidgetItemIterator allocation(m_pt);
    while (*allocation) {
      (*allocation)
          ->setCheckState(0, source.ptIds.contains(
                                 (*allocation)->data(0, Qt::UserRole).toInt())
                                 ? Qt::Checked
                                 : Qt::Unchecked);
      ++allocation;
    }
    rebuildPrimary(source.primaryPtId);
    m_verifications->setRowCount(0);
    for (const RequirementVerification &verification : source.verifications)
      appendVerification(verification);
    m_code->setText(m_service.suggestCode(source.primaryPtId));
    m_loading = false;
    m_dirty = true;
  });
  connect(m_obsoleteButton, &QPushButton::clicked, this,
          &RequirementWidget::obsolete);
  connect(m_save, &QPushButton::clicked, this, &RequirementWidget::save);
  connect(m_cancel, &QPushButton::clicked, this, &RequirementWidget::cancel);
  connect(m_list, &QTableWidget::cellClicked, this,
          &RequirementWidget::selectRow);
  connect(m_list->horizontalHeader(), &QHeaderView::sectionClicked, this,
          &RequirementWidget::sortByColumn);
  connect(m_splitter, &QSplitter::splitterMoved, this, [this] {
    QSettings().setValue("Requirements/splitter", m_splitter->saveState());
  });
  connect(m_includeObsolete, &QCheckBox::toggled, this,
          &RequirementWidget::filtersChanged);
  for (QLineEdit *w :
       {m_codeFilter, m_titleFilter, m_descriptionFilter, m_sourceFilter})
    connect(w, &QLineEdit::textChanged, this,
            &RequirementWidget::filtersChanged);
  for (CheckableComboBox *w :
       {m_statusFilter, m_typeFilter, m_ptFilter, m_methodFilter,
        m_applicabilityFilter, m_allocatedFilter, m_tracedFilter,
        m_documentedFilter, m_verifiedFilter})
    connect(w, &CheckableComboBox::selectionChanged, this,
            &RequirementWidget::filtersChanged);
  connect(m_pt, &QTreeWidget::itemChanged, this,
          &RequirementWidget::allocationChanged);
  connect(m_primary, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &RequirementWidget::primaryChanged);
  connect(addVerif, &QPushButton::clicked, this,
          &RequirementWidget::addVerification);
  connect(removeVerif, &QPushButton::clicked, this,
          &RequirementWidget::removeVerification);
  connect(up, &QPushButton::clicked, this,
          &RequirementWidget::moveVerificationUp);
  connect(down, &QPushButton::clicked, this,
          &RequirementWidget::moveVerificationDown);
  connect(m_verifications, &QTableWidget::cellChanged, this, [this] {
    if (!m_loading)
      m_dirty = true;
  });
  auto addRelation = [this](bool upstream) {
    if (m_current < 0) return;
    QSqlDatabase db = QSqlDatabase::database(m_connection);
    QSqlQuery types("SELECT ID,LABEL FROM REQUIREMENT_RELATION_TYPE ORDER BY ID", db);
    QStringList typeLabels; QList<int> typeIds;
    while (types.next()) { typeIds << types.value(0).toInt(); typeLabels << types.value(1).toString(); }
    bool ok = false;
    const QString typeLabel = QInputDialog::getItem(this, "Relation", "Type", typeLabels, 0, false, &ok);
    if (!ok) return;
    const RelationChoice choice = chooseRequirement(this, db, m_current, upstream ? "Choisir l'exigence amont" : "Choisir l'exigence aval");
    if (choice.id < 0) return;
    const int typeId = typeIds[typeLabels.indexOf(typeLabel)];
    const int other = choice.id;
    int source = m_current, target = other;
    if ((upstream && typeId != 3) || (!upstream && typeId == 3)) { source = other; target = m_current; }
    RequirementRelationService service(m_connection);
    const RequirementResult result = service.add(source, target, typeId, choice.comment);
    if (!result.success) QMessageBox::warning(this, "Relation", result.message);
    else { loadRelations(m_current); emit dataChanged(); }
  };
  connect(addUpstream, &QPushButton::clicked, this, [addRelation] { addRelation(true); });
  connect(addDownstream, &QPushButton::clicked, this, [addRelation] { addRelation(false); });
  connect(removeRelation, &QPushButton::clicked, this, [this] {
    const int row = m_relations->currentRow(); if (row < 0) return;
    RequirementRelationService service(m_connection);
    const RequirementResult result = service.remove(m_relations->item(row, 0)->data(Qt::UserRole).toInt());
    if (!result.success) QMessageBox::warning(this, "Relation", result.message);
    else { loadRelations(m_current); emit dataChanged(); }
  });
  connect(m_relations, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
    if (row < 0) return;
    if (column == 3) {
      bool ok = false;
      const QString value = QInputDialog::getMultiLineText(this, "Commentaire", "Commentaire", m_relations->item(row, 3)->text(), &ok);
      if (!ok) return;
      RequirementRelationService service(m_connection);
      const auto result = service.updateComment(m_relations->item(row, 0)->data(Qt::UserRole).toInt(), value);
      if (!result.success) QMessageBox::warning(this, "Relation", result.message); else loadRelations(m_current);
      return;
    }
    const int otherId = m_relations->item(row, 0)->data(Qt::UserRole + 1).toInt();
    openRequirement(otherId);
  });
  connect(m_graphDepth, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] { if (m_current >= 0) loadRelationGraph(m_current); });
  connect(m_showDerivations, &QCheckBox::toggled, this, [this](bool checked) {
    QSettings settings;
    settings.setValue("Requirements/graph/showDerivations", checked);
    settings.sync();
    if (m_current >= 0) loadRelationGraph(m_current);
  });
  connect(m_showDependencies, &QCheckBox::toggled, this, [this](bool checked) {
    QSettings settings;
    settings.setValue("Requirements/graph/showDependencies", checked);
    settings.sync();
    if (m_current >= 0) loadRelationGraph(m_current);
  });
  connect(m_configurations, &QListWidget::itemChanged, this, [this] { if (!m_loading) m_dirty = true; });
  connect(addToDocument, &QPushButton::clicked, this, [this] {
    if (m_current < 0) return;
    DocumentService service(m_connection); const auto documents = service.documents();
    QStringList labels; for (const auto &doc : documents) labels << doc.reference + " — " + doc.title;
    bool ok = false; const QString selected = QInputDialog::getItem(this, "Document", "Document", labels, 0, false, &ok);
    const int index = labels.indexOf(selected); if (!ok || index < 0) return;
    const auto nodes = service.nodes(documents[index].id); QStringList chapters{"— Racine —"}; QList<int> chapterIds{-1};
    for (const auto &node : nodes) if (node.type == "CHAPTER") { chapters << node.title; chapterIds << node.id; }
    const QString chapter = QInputDialog::getItem(this, "Document", "Chapitre", chapters, 0, false, &ok);
    if (!ok) return;
    const RequirementResult result = service.placeRequirement(documents[index].id, chapterIds[chapters.indexOf(chapter)], m_current);
    if (!result.success) QMessageBox::warning(this, "Document", result.message);
    else { loadDocuments(m_current); emit dataChanged(); }
  });
  connect(removeFromDocument, &QPushButton::clicked, this, [this] {
    const int row = m_documents->currentRow(); if (row < 0) return;
    DocumentService service(m_connection); const RequirementResult result = service.removeNode(m_documents->item(row,0)->data(Qt::UserRole).toInt());
    if (!result.success) QMessageBox::warning(this, "Document", result.message);
    else { loadDocuments(m_current); emit dataChanged(); }
  });
  connect(m_documents, &QTableWidget::cellDoubleClicked, this, [this](int row) {
    if (row >= 0)
      emit openDocumentRequested(
          m_documents->item(row, 0)->data(Qt::UserRole + 1).toInt());
  });
  for (auto *w : {m_code, m_title, m_source})
    connect(w, &QLineEdit::textEdited, this, [this] { m_dirty = true; });
  connect(m_description, &QTextEdit::textChanged, this, [this] {
    if (!m_loading)
      m_dirty = true;
  });
  connect(m_status, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this] {
            if (!m_loading)
              m_dirty = true;
          });
  connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this] {
            if (!m_loading)
              m_dirty = true;
          });
  auto saveHeader = [this] {
    QSettings().setValue("Requirements/header",
                         m_list->horizontalHeader()->saveState());
  };
  connect(m_list->horizontalHeader(), &QHeaderView::sectionResized, this,
          saveHeader);
  connect(m_list->horizontalHeader(), &QHeaderView::sectionMoved, this,
          saveHeader);
  m_list->horizontalHeader()->restoreState(
      QSettings().value("Requirements/header").toByteArray());
  setEditorEnabled(false);
}

static void fillCombo(QComboBox *c, QSqlDatabase db, const QString &sql,
                      const QString &all) {
  c->blockSignals(true);
  c->clear();
  if (!all.isNull())
    c->addItem(all, -1);
  QSqlQuery q(sql, db);
  while (q.next())
    c->addItem(q.value(1).toString(), q.value(0));
  c->blockSignals(false);
}
static void fillCheckable(CheckableComboBox *combo, QSqlDatabase db,
                          const QString &sql) {
  combo->clearItems();
  QSqlQuery query(sql, db);
  while (query.next())
    combo->addItem(query.value(1).toString(), query.value(0).toInt());
}
void RequirementWidget::setConnectionName(const QString &name) {
  m_connection = name;
  m_service.setConnectionName(name);
  const QSqlDatabase db = QSqlDatabase::database(name, false);
  if (name.isEmpty() || !db.isValid() || !db.isOpen()) {
    m_loading = true;
    m_records.clear();
    m_list->setRowCount(1);
    m_current = -1;
    clearEditor();
    setEditorEnabled(false);
    m_loading = false;
    return;
  }
  m_loading = true;
  loadLookups();
  loadPt();
  QSettings settings;
  m_codeFilter->setText(settings.value("Requirements/filter/code").toString());
  m_titleFilter->setText(
      settings.value("Requirements/filter/title").toString());
  m_descriptionFilter->setText(
      settings.value("Requirements/filter/description").toString());
  m_sourceFilter->setText(
      settings.value("Requirements/filter/source").toString());
  auto restoreIds = [&settings](const QString &key) {
    QList<int> ids;
    for (const QVariant &value : settings.value(key).toList())
      ids << value.toInt();
    return ids;
  };
  m_statusFilter->setCheckedIds(restoreIds("Requirements/filter/statuses"));
  m_typeFilter->setCheckedIds(restoreIds("Requirements/filter/types"));
  m_ptFilter->setCheckedIds(restoreIds("Requirements/filter/pts"));
  m_methodFilter->setCheckedIds(restoreIds("Requirements/filter/methods"));
  m_applicabilityFilter->setCheckedIds(
      restoreIds("Requirements/filter/configurations"));
  m_allocatedFilter->setCheckedIds(restoreIds("Requirements/filter/allocated"));
  m_tracedFilter->setCheckedIds(restoreIds("Requirements/filter/traced"));
  m_documentedFilter->setCheckedIds(
      restoreIds("Requirements/filter/documented"));
  m_verifiedFilter->setCheckedIds(restoreIds("Requirements/filter/verified"));
  m_includeObsolete->setChecked(
      settings.value("Requirements/filter/obsolete", false).toBool());
  m_sortColumn = settings.value("Requirements/sort/column", CodeCol).toInt();
  m_sortOrder = static_cast<Qt::SortOrder>(
      settings.value("Requirements/sort/order", Qt::AscendingOrder).toInt());
  m_current = -1;
  m_list->clearSelection();
  clearEditor();
  setEditorEnabled(false);
  m_loading = false;
  refresh();
}
void RequirementWidget::loadLookups() {
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  fillCheckable(m_statusFilter, db,
                "SELECT ID,STATUS FROM REQ_STATUS ORDER BY ID");
  fillCheckable(m_typeFilter, db,
                "SELECT ID,COALESCE(CODE||' — ','')||TYPE FROM REQ_TYPE ORDER BY ID");
  fillCheckable(m_methodFilter, db,
                "SELECT ID,METHOD FROM REQ_METHOD ORDER BY ID");
  fillCheckable(m_applicabilityFilter, db,
                "SELECT ID,CODE FROM CONFIGURATION WHERE ACTIVE=1 "
                "ORDER BY POSITION,CODE");
  fillCombo(m_status, db, "SELECT ID,STATUS FROM REQ_STATUS ORDER BY ID",
            QString());
  fillCombo(
      m_type, db,
      "SELECT ID,COALESCE(CODE||' — ','')||TYPE FROM REQ_TYPE ORDER BY ID",
      QString());
}
void RequirementWidget::loadPt() {
  m_pt->blockSignals(true);
  m_pt->clear();
  m_ptFilter->blockSignals(true);
  m_ptFilter->clearItems();
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  ProductTreeService service(m_connection);
  struct Row {
    int id, parent;
    QString code, description;
  };
  QList<Row> rows;
  QMap<int, QTreeWidgetItem *> items;
  QSqlQuery q("SELECT ID,PARENT,DESCRIPTION FROM PT WHERE ARCHIVED=0 ORDER BY "
              "IFNULL(PARENT,-1),POSITION,SEGMENT",
              db);
  while (q.next()) {
    Row r{q.value(0).toInt(), q.value(1).isNull() ? -1 : q.value(1).toInt(),
          service.fullCode(q.value(0).toInt()), q.value(2).toString()};
    rows << r;
    auto item = new QTreeWidgetItem(
        {r.code + (r.description.isEmpty() ? "" : " — " + r.description)});
    item->setData(0, Qt::UserRole, r.id);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, Qt::Unchecked);
    items[r.id] = item;
  }
  for (const Row &r : rows) {
    if (items.contains(r.parent))
      items[r.parent]->addChild(items[r.id]);
    else
      m_pt->addTopLevelItem(items[r.id]);
  }
  for (const Row &r : rows) {
    int depth = 0;
    for (QTreeWidgetItem *parent = items[r.id]->parent(); parent;
         parent = parent->parent())
      ++depth;
    m_ptFilter->addItem(QString(depth * 3, QChar(' ')) + r.code, r.id,
                        r.description);
  }
  m_pt->expandAll();
  m_ptFilter->blockSignals(false);
  m_pt->blockSignals(false);
}
void RequirementWidget::refresh() {
  const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
  if (!db.isValid() || !db.isOpen()) {
    m_records.clear();
    populateList();
    return;
  }
  m_filter.code = m_codeFilter->text();
  m_filter.title = m_titleFilter->text();
  m_filter.description = m_descriptionFilter->text();
  m_filter.source = m_sourceFilter->text();
  m_filter.statusIds = m_statusFilter->checkedIds();
  m_filter.typeIds = m_typeFilter->checkedIds();
  m_filter.ptIds = m_ptFilter->checkedIds();
  m_filter.methodIds = m_methodFilter->checkedIds();
  m_filter.configurationIds = m_applicabilityFilter->checkedIds();
  m_filter.includeObsolete = m_includeObsolete->isChecked();
  auto coverageValue = [](CheckableComboBox *combo) {
    const QList<int> values = combo->checkedIds();
    return values.size() == 1 ? values.first() : -1;
  };
  m_filter.allocated = coverageValue(m_allocatedFilter);
  m_filter.traced = coverageValue(m_tracedFilter);
  m_filter.documented = coverageValue(m_documentedFilter);
  m_filter.verified = coverageValue(m_verifiedFilter);
  QString error;
  m_records = m_service.find(m_filter, &error);
  sortByColumn(m_sortColumn);
  if (m_current >= 0) {
    loadChanges(m_current);
    loadHistory(m_current);
  }
}
void RequirementWidget::openRequirement(int id) {
  if (id < 0 || !confirmDiscard())
    return;
  auto rowForId = [this, id]() {
    for (int row = 1; row < m_list->rowCount(); ++row) {
      QTableWidgetItem *item = m_list->item(row, CodeCol);
      if (item && item->data(Qt::UserRole).toInt() == id)
        return row;
    }
    return -1;
  };
  int row = rowForId();
  if (row < 0) {
    RequirementFilter all;
    all.includeObsolete = true;
    applyFilter(all);
    row = rowForId();
  }
  if (row >= 0) {
    m_list->setCurrentCell(row, CodeCol);
    m_list->scrollToItem(m_list->item(row, CodeCol));
  }
  loadEditor(id);
}
void RequirementWidget::openRequirementApplicability(int id) {
  openRequirement(id);
  if (m_current != id)
    return;
  for (int index = 0; index < m_tabs->count(); ++index) {
    if (m_tabs->tabText(index) == "Applicabilité") {
      m_tabs->setCurrentIndex(index);
      break;
    }
  }
}
void RequirementWidget::sortByColumn(int column) {
  if (sender() == m_list->horizontalHeader()) {
    if (m_sortColumn == column)
      m_sortOrder = m_sortOrder == Qt::AscendingOrder ? Qt::DescendingOrder
                                                      : Qt::AscendingOrder;
    else
      m_sortOrder = Qt::AscendingOrder;
  }
  m_sortColumn = column;
  auto text = [column](const RequirementRecord &r) {
    switch (column) {
    case CodeCol:
      return r.code;
    case TitleCol:
      return r.title;
    case StatusCol:
      return QString::number(r.statusId);
    case TypeCol:
      return QString::number(r.typeId);
    case PtCol:
      return r.productTrees;
    case MethodCol:
      return r.verificationMethods;
    case DescriptionCol:
      return r.description;
    case ApplicabilityCol:
      return r.applicability;
    case SourceCol:
      return r.source;
    case AllocatedCol:
      return QString::number(r.allocated);
    case TracedCol:
      return QString::number(r.traced);
    case DocumentedCol:
      return QString::number(r.documented);
    case VerifiedCol:
      return QString::number(r.verified);
    default:
      return QString();
    }
  };
  std::sort(m_records.begin(), m_records.end(),
            [&](const auto &a, const auto &b) {
              int cmp = QString::localeAwareCompare(text(a), text(b));
              return m_sortOrder == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
            });
  m_list->horizontalHeader()->setSortIndicator(column, m_sortOrder);
  QSettings().setValue("Requirements/sort/column", m_sortColumn);
  QSettings().setValue("Requirements/sort/order", m_sortOrder);
  populateList();
}
void RequirementWidget::populateList() {
  m_list->setRowCount(m_records.size() + 1);
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  for (int row = 0; row < m_records.size(); ++row) {
    const auto &r = m_records[row];
    QString status, type;
    QSqlQuery q(db);
    q.prepare("SELECT (SELECT STATUS FROM REQ_STATUS WHERE ID=?),(SELECT "
              "COALESCE(CODE||' — ','')||TYPE FROM REQ_TYPE WHERE ID=?)");
    q.addBindValue(r.statusId);
    q.addBindValue(r.typeId);
    if (q.exec() && q.next()) {
      status = q.value(0).toString();
      type = q.value(1).toString();
    }
    QStringList cells = {r.code,        r.title,         status,
                         type,          r.productTrees,  r.verificationMethods,
                         r.description, r.applicability, r.source,
                         r.allocated ? "✓" : "—", r.traced ? "✓" : "—",
                         r.documented ? "✓" : "—", r.verified ? "✓" : "—"};
    for (int c = 0; c < ColumnCount; ++c) {
      auto item = new QTableWidgetItem(cells[c]);
      item->setData(Qt::UserRole, r.id);
      m_list->setItem(row + 1, c, item);
    }
  }
  if (QSettings().value("Requirements/header").toByteArray().isEmpty())
    for (int i = 0; i < ColumnCount; ++i)
      m_list->setColumnWidth(i, i == DescriptionCol ? 220 : 130);
}
void RequirementWidget::clearEditor() {
  m_loading = true;
  m_code->clear();
  m_title->clear();
  m_description->clear();
  m_source->clear();
  m_codeWarning->clear();
  if (m_status->count())
    m_status->setCurrentIndex(0);
  if (m_type->count())
    m_type->setCurrentIndex(0);
  QTreeWidgetItemIterator it(m_pt);
  while (*it) {
    (*it)->setCheckState(0, Qt::Unchecked);
    ++it;
  }
  m_primary->clear();
  m_verifications->setRowCount(0);
  m_relations->setRowCount(0);
  if (m_relationGraph->scene()) m_relationGraph->scene()->deleteLater();
  m_relationGraph->setScene(new QGraphicsScene(m_relationGraph));
  m_documents->setRowCount(0);
  m_changes->setRowCount(0);
  m_history->setRowCount(0);
  loadApplicability({});
  m_loading = false;
  m_dirty = false;
}
void RequirementWidget::setEditorEnabled(bool enabled) {
  m_editor->setEnabled(enabled);
  m_save->setEnabled(enabled);
  m_cancel->setEnabled(enabled);
  m_duplicate->setEnabled(enabled && m_current >= 0);
  m_obsoleteButton->setEnabled(enabled && m_current >= 0);
}
bool RequirementWidget::confirmDiscard() {
  return !m_dirty ||
         QMessageBox::question(this, "Modifications non enregistrées",
                               "Abandonner les modifications en cours ?",
                               QMessageBox::Yes | QMessageBox::No) ==
             QMessageBox::Yes;
}
void RequirementWidget::selectRow(int row, int) {
  if (row == 0)
    return;
  int id = m_list->item(row, CodeCol)->data(Qt::UserRole).toInt();
  if (id == m_current)
    return;
  if (!confirmDiscard())
    return;
  loadEditor(id);
}
void RequirementWidget::loadEditor(int id) {
  m_loading = true;
  m_current = id;
  RequirementRecord r = m_service.get(id);
  m_code->setText(r.code);
  m_title->setText(r.title);
  m_description->setPlainText(r.description);
  m_source->setText(r.source);
  m_status->setCurrentIndex(qMax(0, m_status->findData(r.statusId)));
  m_type->setCurrentIndex(qMax(0, m_type->findData(r.typeId)));
  QTreeWidgetItemIterator it(m_pt);
  while (*it) {
    (*it)->setCheckState(0,
                         r.ptIds.contains((*it)->data(0, Qt::UserRole).toInt())
                             ? Qt::Checked
                             : Qt::Unchecked);
    ++it;
  }
  rebuildPrimary(r.primaryPtId);
  m_verifications->setRowCount(0);
  for (const auto &v : r.verifications)
    appendVerification(v);
  loadRelations(id);
  loadRelationGraph(id);
  loadDocuments(id);
  loadApplicability(r.configurationIds);
  loadChanges(id);
  loadHistory(id);
  ProductTreeService pt(m_connection);
  QString prefix = pt.fullCode(r.primaryPtId) + "-R-";
  m_codeWarning->setText(
      r.primaryPtId >= 0 && !r.code.startsWith(prefix)
          ? "Le préfixe du code ne correspond plus au PT principal. Le code "
            "reste volontairement inchangé."
          : QString());
  m_loading = false;
  m_dirty = false;
  setEditorEnabled(true);
}
void RequirementWidget::create() {
  if (!confirmDiscard())
    return;
  m_current = -1;
  m_list->clearSelection();
  clearEditor();
  setEditorEnabled(true);
  m_duplicate->setEnabled(false);
  m_obsoleteButton->setEnabled(false);
  m_code->setFocus();
}
void RequirementWidget::cancel() {
  if (!confirmDiscard())
    return;
  if (m_current >= 0)
    loadEditor(m_current);
  else {
    clearEditor();
    setEditorEnabled(false);
  }
}
void RequirementWidget::allocationChanged() {
  if (m_loading)
    return;
  int preferred = m_primary->currentData().toInt();
  rebuildPrimary(preferred);
  if (m_current < 0 && m_code->text().isEmpty() &&
      m_primary->currentData().toInt() >= 0)
    m_code->setText(m_service.suggestCode(m_primary->currentData().toInt()));
  m_dirty = true;
}
void RequirementWidget::rebuildPrimary(int preferred) {
  m_primary->blockSignals(true);
  m_primary->clear();
  QTreeWidgetItemIterator it(m_pt);
  while (*it) {
    if ((*it)->checkState(0) == Qt::Checked)
      m_primary->addItem((*it)->text(0).section(" — ", 0, 0),
                         (*it)->data(0, Qt::UserRole));
    ++it;
  }
  int index = m_primary->findData(preferred);
  m_primary->setCurrentIndex(index >= 0 ? index
                                        : (m_primary->count() ? 0 : -1));
  m_primary->setEnabled(m_primary->count() > 1);
  m_primary->blockSignals(false);
}
void RequirementWidget::primaryChanged(int) {
  if (!m_loading)
    m_dirty = true;
}
QComboBox *RequirementWidget::methodCombo(int selected) const {
  auto *c = new QComboBox;
  fillCombo(c, QSqlDatabase::database(m_connection),
            "SELECT ID,METHOD FROM REQ_METHOD ORDER BY ID", QString());
  int i = c->findData(selected);
  if (i >= 0)
    c->setCurrentIndex(i);
  connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged),
          const_cast<RequirementWidget *>(this), [this] {
            if (!m_loading)
              const_cast<RequirementWidget *>(this)->m_dirty = true;
          });
  return c;
}
QComboBox *RequirementWidget::levelCombo(int selected,
                                         const QString &legacy) const {
  auto *combo = new QComboBox;
  combo->addItem("— Aucun niveau —", -1);
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  ProductTreeService service(m_connection);
  struct Node {
    int id;
    int parent;
    int position;
    QString segment;
    QString description;
    bool archived;
  };
  QList<Node> nodes;
  QMultiMap<int, Node> children;
  QSqlQuery query("SELECT ID,PARENT,POSITION,SEGMENT,DESCRIPTION,ARCHIVED FROM PT "
                  "ORDER BY POSITION,SEGMENT",
                  db);
  while (query.next()) {
    Node node{query.value(0).toInt(),
              query.value(1).isNull() ? -1 : query.value(1).toInt(),
              query.value(2).toInt(), query.value(3).toString(),
              query.value(4).toString(), query.value(5).toBool()};
    children.insert(node.parent, node);
  }
  std::function<void(int, int)> append = [&](int parent, int depth) {
    QList<Node> siblings = children.values(parent);
    std::sort(siblings.begin(), siblings.end(), [](const Node &a, const Node &b) {
      if (a.position != b.position)
        return a.position < b.position;
      return a.segment.localeAwareCompare(b.segment) < 0;
    });
    for (const Node &node : siblings) {
      if (!node.archived || node.id == selected) {
        QString label = QString(depth * 3, QChar(' ')) + service.fullCode(node.id);
        if (!node.description.isEmpty())
          label += " — " + node.description;
        if (node.archived)
          label += " [archivé]";
        combo->addItem(label, node.id);
      }
      append(node.id, depth + 1);
    }
  };
  append(-1, 0);
  if (selected < 0 && !legacy.trimmed().isEmpty())
    combo->addItem("Non résolu : " + legacy, -1);
  const int index = combo->findData(selected);
  combo->setCurrentIndex(index >= 0 ? index : 0);
  connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          const_cast<RequirementWidget *>(this), [this] {
            if (!m_loading)
              const_cast<RequirementWidget *>(this)->m_dirty = true;
          });
  return combo;
}
QComboBox *RequirementWidget::verdictCombo(const QString &selected) const {
  auto *c = new QComboBox;
  c->addItem("", QString());
  c->addItem("C — Conforme", "C");
  c->addItem("PC — Partiellement conforme", "PC");
  c->addItem("NC — Non conforme", "NC");
  c->setToolTip(
      "C : Conforme • PC : Partiellement conforme • NC : Non conforme");
  int i = c->findData(selected);
  if (i >= 0)
    c->setCurrentIndex(i);
  connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged),
          const_cast<RequirementWidget *>(this), [this] {
            if (!m_loading)
              const_cast<RequirementWidget *>(this)->m_dirty = true;
          });
  return c;
}
void RequirementWidget::appendVerification(const RequirementVerification &v) {
  int row = m_verifications->rowCount();
  m_verifications->insertRow(row);
  m_verifications->setCellWidget(row, 0, methodCombo(v.methodId));
  m_verifications->setCellWidget(row, 1, levelCombo(v.levelPtId, v.level));
  QStringList values = {v.procedure, v.redmine, v.means};
  for (int c = 2; c <= 4; ++c)
    m_verifications->setItem(row, c, new QTableWidgetItem(values[c - 2]));
  m_verifications->setCellWidget(row, 5, verdictCombo(v.verdict));
  m_verifications->setItem(row, 6, new QTableWidgetItem(v.comment));
}
void RequirementWidget::addVerification() {
  appendVerification();
  m_verifications->selectRow(m_verifications->rowCount() - 1);
  m_dirty = true;
}
void RequirementWidget::removeVerification() {
  int row = m_verifications->currentRow();
  if (row >= 0) {
    m_verifications->removeRow(row);
    m_dirty = true;
  }
}
static void swapVerificationRows(QTableWidget *t, int a, int b) {
  for (int c = 0; c < t->columnCount(); ++c) {
    QWidget *wa = t->cellWidget(a, c), *wb = t->cellWidget(b, c);
    QTableWidgetItem *ia = t->takeItem(a, c), *ib = t->takeItem(b, c);
    if (wa)
      t->removeCellWidget(a, c);
    if (wb)
      t->removeCellWidget(b, c);
    if (ia)
      t->setItem(b, c, ia);
    if (ib)
      t->setItem(a, c, ib);
    if (wa)
      t->setCellWidget(b, c, wa);
    if (wb)
      t->setCellWidget(a, c, wb);
  }
}
void RequirementWidget::moveVerificationUp() {
  int row = m_verifications->currentRow();
  if (row > 0) {
    swapVerificationRows(m_verifications, row, row - 1);
    m_verifications->selectRow(row - 1);
    m_dirty = true;
  }
}
void RequirementWidget::moveVerificationDown() {
  int row = m_verifications->currentRow();
  if (row >= 0 && row + 1 < m_verifications->rowCount()) {
    swapVerificationRows(m_verifications, row, row + 1);
    m_verifications->selectRow(row + 1);
    m_dirty = true;
  }
}
RequirementRecord RequirementWidget::editorRecord() const {
  RequirementRecord r;
  r.id = m_current;
  r.code = m_code->text();
  r.title = m_title->text();
  r.description = m_description->toPlainText();
  r.source = m_source->text();
  r.statusId = m_status->currentData().toInt();
  r.typeId = m_type->currentData().toInt();
  r.primaryPtId = m_primary->currentData().toInt();
  QTreeWidgetItemIterator it(m_pt);
  while (*it) {
    if ((*it)->checkState(0) == Qt::Checked)
      r.ptIds << (*it)->data(0, Qt::UserRole).toInt();
    ++it;
  }
  for (int row = 0; row < m_configurations->count(); ++row) {
    QListWidgetItem *item = m_configurations->item(row);
    if (item->checkState() == Qt::Checked)
      r.configurationIds << item->data(Qt::UserRole).toInt();
  }
  for (int row = 0; row < m_verifications->rowCount(); ++row) {
    RequirementVerification v;
    v.methodId = qobject_cast<QComboBox *>(m_verifications->cellWidget(row, 0))
                     ->currentData()
                     .toInt();
    auto text = [&](int c) {
      auto *i = m_verifications->item(row, c);
      return i ? i->text() : QString();
    };
    auto *level = qobject_cast<QComboBox *>(m_verifications->cellWidget(row, 1));
    v.levelPtId = level ? level->currentData().toInt() : -1;
    v.level = v.levelPtId < 0 && level ? level->currentText().remove("Non résolu : ")
                                      : QString();
    v.procedure = text(2);
    v.redmine = text(3);
    v.means = text(4);
    v.verdict = qobject_cast<QComboBox *>(m_verifications->cellWidget(row, 5))
                    ->currentData()
                    .toString();
    v.comment = text(6);
    v.position = row;
    r.verifications << v;
  }
  return r;
}
void RequirementWidget::save() {
  auto result = m_service.save(editorRecord());
  if (!result.success) {
    QMessageBox::warning(this, "Exigence", result.message);
    return;
  }
  m_current = result.id;
  m_dirty = false;
  refresh();
  loadEditor(m_current);
  emit dataChanged();
}

void RequirementWidget::loadRelations(int id) {
  m_relations->setRowCount(0);
  RequirementRelationService service(m_connection);
  for (const RequirementRelationRecord &relation : service.relations(id)) {
    const int row = m_relations->rowCount();
    m_relations->insertRow(row);
    QString label;
    if (relation.typeCode == "DECOMPOSE")
      label = relation.outgoing ? "Enfant" : "Parent";
    else if (relation.typeCode == "DERIVES_FROM")
      label = relation.outgoing ? "Dérivée" : "Source";
    else
      label = relation.outgoing ? "Dépend de" : "Dépendante";
    auto *type = new QTableWidgetItem(label);
    type->setData(Qt::UserRole, relation.id);
    type->setData(Qt::UserRole + 1,
                  relation.outgoing ? relation.targetId : relation.sourceId);
    m_relations->setItem(row, 0, type);
    auto *code = new QTableWidgetItem(relation.otherCode);
    auto *title = new QTableWidgetItem(relation.otherTitle);
    const QString details = QString("Statut : %1\nType : %2\nPT : %3")
                                .arg(relation.otherStatus, relation.otherType,
                                     relation.otherProductTrees);
    code->setToolTip(details); title->setToolTip(details);
    m_relations->setItem(row, 1, code);
    m_relations->setItem(row, 2, title);
    m_relations->setItem(row, 3, new QTableWidgetItem(relation.comment));
  }
}

void RequirementWidget::loadRelationGraph(int id) {
  if (m_relationGraph->scene())
    m_relationGraph->scene()->deleteLater();
  auto *scene = new QGraphicsScene(m_relationGraph);
  m_relationGraph->setScene(scene);
  if (id < 0)
    return;

  enum GraphZone { DecompositionZone, DerivationZone, DependencyZone };
  struct GraphNode {
    int id = -1;
    QString code;
    int distance = 0;
    GraphZone zone = DecompositionZone;
    int predecessor = -1;
    QString viaType;
  };
  struct GraphEdge {
    int id = -1;
    int source = -1;
    int target = -1;
    QString type;
  };

  QHash<int, GraphNode> nodes;
  nodes.insert(id, GraphNode{id, m_code->text(), 0, DecompositionZone, -1, {}});
  QList<int> frontier{id};
  QList<GraphEdge> edges;
  QSet<int> edgeIds;
  QSqlDatabase db = QSqlDatabase::database(m_connection);
  for (int depth = 1;
       depth <= m_graphDepth->value() && !frontier.isEmpty(); ++depth) {
    QList<int> next;
    for (int current : std::as_const(frontier)) {
      QSqlQuery query(db);
      query.prepare(
          "SELECT L.ID,L.SOURCE_REQ_ID,L.TARGET_REQ_ID,T.CODE,"
          "CASE WHEN L.SOURCE_REQ_ID=? THEN TR.ID ELSE SR.ID END,"
          "CASE WHEN L.SOURCE_REQ_ID=? THEN TR.CODE ELSE SR.CODE END "
          "FROM REQUIREMENT_RELATION L "
          "JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=L.TYPE_ID "
          "JOIN REQUIREMENT SR ON SR.ID=L.SOURCE_REQ_ID "
          "JOIN REQUIREMENT TR ON TR.ID=L.TARGET_REQ_ID "
          "WHERE L.SOURCE_REQ_ID=? OR L.TARGET_REQ_ID=? "
          "ORDER BY L.TYPE_ID,6,L.ID");
      query.addBindValue(current);
      query.addBindValue(current);
      query.addBindValue(current);
      query.addBindValue(current);
      if (!query.exec())
        continue;
      while (query.next()) {
        const QString type = query.value(3).toString();
        if ((type == "DERIVES_FROM" && !m_showDerivations->isChecked()) ||
            (type == "DEPENDS_ON" && !m_showDependencies->isChecked()))
          continue;

        const int relationId = query.value(0).toInt();
        if (!edgeIds.contains(relationId)) {
          edgeIds.insert(relationId);
          edges << GraphEdge{relationId, query.value(1).toInt(),
                             query.value(2).toInt(), type};
        }

        const int other = query.value(4).toInt();
        if (nodes.contains(other))
          continue;
        const GraphNode currentNode = nodes.value(current);
        GraphZone zone = currentNode.zone;
        if (type == "DERIVES_FROM")
          zone = DerivationZone;
        else if (type == "DEPENDS_ON")
          zone = DependencyZone;
        nodes.insert(other, GraphNode{other, query.value(5).toString(), depth,
                                      zone, current, type});
        next << other;
      }
    }
    frontier = next;
  }

  // A relation may have been seen from a displayed node while its other end
  // fell outside the requested depth. Such an edge must not be drawn.
  edges.erase(std::remove_if(edges.begin(), edges.end(), [&nodes](const auto &e) {
                return !nodes.contains(e.source) || !nodes.contains(e.target);
              }),
              edges.end());

  constexpr qreal nodeWidth = 190.0;
  constexpr qreal nodeHeight = 64.0;
  constexpr qreal horizontalStep = 245.0;
  constexpr qreal verticalStep = 125.0;

  // Establish vertical ranks from decomposition only. Walking through a
  // parent and back down to one of its other children therefore places that
  // sibling on the same row as the selected requirement.
  QHash<int, int> hierarchyRanks{{id, 0}};
  QList<int> hierarchyFrontier{id};
  while (!hierarchyFrontier.isEmpty()) {
    const int current = hierarchyFrontier.takeFirst();
    for (const GraphEdge &edge : std::as_const(edges)) {
      if (edge.type != "DECOMPOSE")
        continue;
      int other = -1;
      int rank = 0;
      if (edge.source == current) {
        other = edge.target;
        rank = hierarchyRanks.value(current) + 1;
      } else if (edge.target == current) {
        other = edge.source;
        rank = hierarchyRanks.value(current) - 1;
      }
      if (other >= 0 && !hierarchyRanks.contains(other)) {
        hierarchyRanks.insert(other, rank);
        hierarchyFrontier << other;
      }
    }
  }

  QMap<int, QList<int>> hierarchyLayers;
  for (auto it = hierarchyRanks.cbegin(); it != hierarchyRanks.cend(); ++it)
    hierarchyLayers[it.value()] << it.key();
  for (auto it = hierarchyLayers.begin(); it != hierarchyLayers.end(); ++it)
    std::sort(it.value().begin(), it.value().end(), [&nodes](int a, int b) {
      return nodes.value(a).code.localeAwareCompare(nodes.value(b).code) < 0;
    });

  QHash<int, QPointF> positions;
  for (auto it = hierarchyLayers.cbegin(); it != hierarchyLayers.cend(); ++it) {
    QList<int> layer = it.value();
    if (layer.contains(id)) {
      positions.insert(id, QPointF(0, 0));
      layer.removeAll(id);
      for (int index = 0; index < layer.size(); ++index) {
        const int column = index / 2 + 1;
        const int direction = index % 2 == 0 ? -1 : 1;
        positions.insert(layer[index],
                         QPointF(direction * column * horizontalStep, 0));
      }
    } else {
      for (int index = 0; index < layer.size(); ++index) {
        const qreal x =
            (index - (layer.size() - 1) / 2.0) * horizontalStep;
        positions.insert(layer[index], QPointF(x, it.key() * verticalStep));
      }
    }
  }

  qreal hierarchyLeft = -nodeWidth / 2;
  qreal hierarchyRight = nodeWidth / 2;
  for (auto it = positions.cbegin(); it != positions.cend(); ++it) {
    hierarchyLeft = qMin(hierarchyLeft, it.value().x() - nodeWidth / 2);
    hierarchyRight = qMax(hierarchyRight, it.value().x() + nodeWidth / 2);
  }

  // Place non-hierarchical relations beside the node from which they were
  // discovered. Their vertical coordinate follows that anchor; only
  // decomposition changes vertical level.
  for (int distance = 1; distance <= m_graphDepth->value(); ++distance) {
    QMap<QPair<int, int>, QList<int>> lateralGroups;
    for (const GraphNode &node : nodes) {
      if (node.distance != distance || hierarchyRanks.contains(node.id) ||
          !positions.contains(node.predecessor))
        continue;
      const int side = node.zone == DerivationZone ? -1 : 1;
      lateralGroups[qMakePair(node.predecessor, side)] << node.id;
    }
    for (auto it = lateralGroups.begin(); it != lateralGroups.end(); ++it) {
      QList<int> group = it.value();
      std::sort(group.begin(), group.end(), [&nodes](int a, int b) {
        return nodes.value(a).code.localeAwareCompare(nodes.value(b).code) < 0;
      });
      const QPointF anchor = positions.value(it.key().first);
      for (int index = 0; index < group.size(); ++index) {
        const GraphNode node = nodes.value(group[index]);
        QPointF position;
        if (node.viaType == "DECOMPOSE") {
          const GraphEdge relation = *std::find_if(
              edges.cbegin(), edges.cend(), [&node](const GraphEdge &edge) {
                return edge.type == "DECOMPOSE" &&
                       ((edge.source == node.predecessor &&
                         edge.target == node.id) ||
                        (edge.target == node.predecessor &&
                         edge.source == node.id));
              });
          const int direction = relation.source == node.predecessor ? 1 : -1;
          position = QPointF(anchor.x(), anchor.y() + direction * verticalStep);
        } else {
          const int side = node.zone == DerivationZone ? -1 : 1;
          const qreal x = side < 0
                              ? qMin(anchor.x() - horizontalStep,
                                     hierarchyLeft - 80 - nodeWidth / 2)
                              : qMax(anchor.x() + horizontalStep,
                                     hierarchyRight + 80 + nodeWidth / 2);
          const qreal y = anchor.y() +
                          (index - (group.size() - 1) / 2.0) * 85.0;
          position = QPointF(x, y);
        }
        positions.insert(node.id, position);
      }
    }
  }

  for (const GraphNode &node : nodes) {
    const QPointF center = positions.value(node.id);
    const QRectF box(center.x() - nodeWidth / 2, center.y() - nodeHeight / 2,
                     nodeWidth, nodeHeight);
    const QColor fill = node.id == id ? QColor("#ffe49a") : QColor("#f6f6f6");
    auto *rect = scene->addRect(box, QPen(Qt::darkGray), QBrush(fill));
    rect->setData(0, node.id);
    rect->setData(1, "node");
    rect->setData(2, static_cast<int>(node.zone));
    rect->setData(3, node.distance);
    rect->setToolTip("Cliquer pour ouvrir");
    rect->setFlag(QGraphicsItem::ItemIsSelectable);
    auto *textItem = scene->addText(node.code);
    textItem->setTextWidth(nodeWidth - 12);
    textItem->document()->setDefaultTextOption(
        QTextOption(Qt::AlignCenter));
    textItem->setPos(box.left() + 6, center.y() - textItem->boundingRect().height() / 2);
    textItem->setAcceptedMouseButtons(Qt::NoButton);
  }

  auto boxBoundary = [=](const QPointF &center, const QPointF &towards) {
    const QPointF delta = towards - center;
    if (qFuzzyIsNull(delta.x()) && qFuzzyIsNull(delta.y()))
      return center;
    const qreal xScale = qFuzzyIsNull(delta.x())
                             ? std::numeric_limits<qreal>::max()
                             : (nodeWidth / 2) / qAbs(delta.x());
    const qreal yScale = qFuzzyIsNull(delta.y())
                             ? std::numeric_limits<qreal>::max()
                             : (nodeHeight / 2) / qAbs(delta.y());
    return center + delta * qMin(xScale, yScale);
  };
  for (const GraphEdge &edge : std::as_const(edges)) {
    const QPointF sourceCenter = positions.value(edge.source);
    const QPointF targetCenter = positions.value(edge.target);
    const QPointF sourcePoint = boxBoundary(sourceCenter, targetCenter);
    const QPointF targetPoint = boxBoundary(targetCenter, sourceCenter);
    QColor color = edge.type == "DECOMPOSE" ? QColor("#2878b5")
                   : edge.type == "DERIVES_FROM" ? QColor("#378a28")
                                                   : QColor("#b87810");
    QPen pen(color, 2);
    if (edge.type == "DERIVES_FROM")
      pen.setStyle(Qt::DashLine);
    else if (edge.type == "DEPENDS_ON")
      pen.setStyle(Qt::DotLine);
    auto *line = scene->addLine(QLineF(sourcePoint, targetPoint), pen);
    line->setData(1, "edge");
    line->setData(2, edge.type);
    line->setData(3, edge.source);
    line->setData(4, edge.target);
    line->setZValue(-1);

    QLineF direction(sourcePoint, targetPoint);
    if (direction.length() > 0) {
      const QPointF unit = (targetPoint - sourcePoint) / direction.length();
      const QPointF perpendicular(-unit.y(), unit.x());
      QPolygonF arrow;
      arrow << targetPoint << targetPoint - unit * 12 + perpendicular * 5
            << targetPoint - unit * 12 - perpendicular * 5;
      auto *head = scene->addPolygon(arrow, QPen(color), QBrush(color));
      head->setData(1, "arrow");
      head->setData(2, edge.type);
      head->setZValue(-0.5);
    }
  }
  connect(scene, &QGraphicsScene::selectionChanged, this, [this, scene, id] {
    const auto selected = scene->selectedItems();
    if (selected.isEmpty()) return;
    const int target = selected.first()->data(0).toInt();
    if (target >= 0 && target != id)
      QTimer::singleShot(0, this, [this, target] { openRequirement(target); });
  });
  scene->setSceneRect(scene->itemsBoundingRect().adjusted(-20, -20, 20, 20));
}

void RequirementWidget::loadDocuments(int id) {
  m_documents->setRowCount(0);
  QSqlQuery query(QSqlDatabase::database(m_connection));
  query.prepare("SELECT N.ID,D.TITLE,COALESCE(D.REFERENCE,''),"
                "COALESCE(P.TITLE,'Racine') FROM DOCUMENT_NODE N "
                "JOIN DOCUMENT D ON D.ID=N.DOC_ID LEFT JOIN DOCUMENT_NODE P "
                "ON P.ID=N.PARENT_ID WHERE N.REQ_ID=? AND "
                "N.NODE_TYPE='REQUIREMENT' ORDER BY D.TITLE");
  query.addBindValue(id);
  if (!query.exec())
    return;
  while (query.next()) {
    const int row = m_documents->rowCount();
    m_documents->insertRow(row);
    auto *document = new QTableWidgetItem(query.value(1).toString());
    document->setData(Qt::UserRole, query.value(0));
    QSqlQuery documentId(QSqlDatabase::database(m_connection));
    documentId.prepare("SELECT DOC_ID FROM DOCUMENT_NODE WHERE ID=?");
    documentId.addBindValue(query.value(0));
    if (documentId.exec() && documentId.next())
      document->setData(Qt::UserRole + 1, documentId.value(0));
    m_documents->setItem(row, 0, document);
    m_documents->setItem(row, 1,
                         new QTableWidgetItem(query.value(2).toString()));
    m_documents->setItem(row, 2,
                         new QTableWidgetItem(query.value(3).toString()));
  }
}

void RequirementWidget::loadApplicability(const QList<int> &selected) {
  const bool wasLoading = m_loading;
  m_loading = true;
  m_configurations->clear();
  if (!m_connection.isEmpty()) {
    QSqlQuery query(
        "SELECT ID,CODE,LABEL,ACTIVE FROM CONFIGURATION "
        "ORDER BY POSITION,CODE",
        QSqlDatabase::database(m_connection));
    while (query.next()) {
      const int id = query.value(0).toInt();
      if (!query.value(3).toBool() && !selected.contains(id))
        continue;
      auto *item = new QListWidgetItem(
          query.value(1).toString() + " — " + query.value(2).toString() +
          (query.value(3).toBool() ? "" : " [archivée]"));
      item->setData(Qt::UserRole, id);
      item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
      item->setCheckState(selected.contains(id) ? Qt::Checked : Qt::Unchecked);
      if (!query.value(3).toBool()) {
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        item->setToolTip(
            "Configuration archivée : restaurez-la depuis Projet > "
            "Applicabilité pour modifier cette association.");
      }
      m_configurations->addItem(item);
    }
  }
  m_loading = wasLoading;
}

void RequirementWidget::loadChanges(int id) {
  m_changes->setRowCount(0);
  ChangeFilter filter;
  filter.objectType = "REQUIREMENT";
  filter.objectId = id;
  filter.includeArchived = true;
  const auto rows = ChangeService(m_connection).find(filter);
  for (const ChangeRecord &record : rows) {
    const int row = m_changes->rowCount();
    m_changes->insertRow(row);
    const QStringList values = {
        record.code, record.typeLabel, record.statusLabel, record.description,
        record.decision};
    for (int column = 0; column < values.size(); ++column)
      m_changes->setItem(row, column, new QTableWidgetItem(values[column]));
    m_changes->item(row, 0)->setData(Qt::UserRole, record.id);
    if (record.archived)
      for (int column = 0; column < m_changes->columnCount(); ++column)
        m_changes->item(row, column)->setForeground(Qt::gray);
  }
  m_changes->resizeColumnsToContents();
}

void RequirementWidget::loadHistory(int id) {
  m_history->setRowCount(0);
  const auto rows = HistoryService(m_connection).find({}, "REQUIREMENT", {}, id);
  for (const HistoryRecord &record : rows) {
    const int row = m_history->rowCount();
    m_history->insertRow(row);
    const QStringList values = {record.time, record.author, record.eventType,
                                record.beforeJson, record.afterJson,
                                record.comment};
    for (int column = 0; column < values.size(); ++column)
      m_history->setItem(row, column, new QTableWidgetItem(values[column]));
  }
  m_history->resizeColumnsToContents();
}
void RequirementWidget::obsolete() {
  if (m_current < 0)
    return;
  auto result = m_service.setObsolete(m_current);
  if (!result.success)
    QMessageBox::warning(this, "Exigence", result.message);
  else {
    m_current = -1;
    clearEditor();
    setEditorEnabled(false);
    refresh();
    emit dataChanged();
  }
}
void RequirementWidget::filtersChanged() {
  if (m_loading)
    return;
  QSettings settings;
  settings.setValue("Requirements/filter/code", m_codeFilter->text());
  settings.setValue("Requirements/filter/title", m_titleFilter->text());
  settings.setValue("Requirements/filter/description",
                    m_descriptionFilter->text());
  settings.setValue("Requirements/filter/source", m_sourceFilter->text());
  auto variants = [](const QList<int> &ids) {
    QVariantList result;
    for (int id : ids)
      result << id;
    return result;
  };
  settings.setValue("Requirements/filter/statuses", variants(m_statusFilter->checkedIds()));
  settings.setValue("Requirements/filter/types", variants(m_typeFilter->checkedIds()));
  settings.setValue("Requirements/filter/pts", variants(m_ptFilter->checkedIds()));
  settings.setValue("Requirements/filter/methods", variants(m_methodFilter->checkedIds()));
  settings.setValue("Requirements/filter/configurations", variants(m_applicabilityFilter->checkedIds()));
  settings.setValue("Requirements/filter/allocated", variants(m_allocatedFilter->checkedIds()));
  settings.setValue("Requirements/filter/traced", variants(m_tracedFilter->checkedIds()));
  settings.setValue("Requirements/filter/documented", variants(m_documentedFilter->checkedIds()));
  settings.setValue("Requirements/filter/verified", variants(m_verifiedFilter->checkedIds()));
  settings.setValue("Requirements/filter/obsolete",
                    m_includeObsolete->isChecked());
  refresh();
}
void RequirementWidget::applyFilter(const RequirementFilter &filter) {
  m_filter = filter;
  m_loading = true;
  m_codeFilter->setText(filter.code);
  m_titleFilter->setText(filter.title);
  m_descriptionFilter->setText(filter.description);
  m_sourceFilter->setText(filter.source);
  m_statusFilter->setCheckedIds(filter.statusIds);
  m_typeFilter->setCheckedIds(filter.typeIds);
  m_ptFilter->setCheckedIds(filter.ptIds);
  m_methodFilter->setCheckedIds(filter.methodIds);
  m_applicabilityFilter->setCheckedIds(filter.configurationIds);
  m_allocatedFilter->setCheckedIds(filter.allocated < 0 ? QList<int>() : QList<int>{filter.allocated});
  m_tracedFilter->setCheckedIds(filter.traced < 0 ? QList<int>() : QList<int>{filter.traced});
  m_documentedFilter->setCheckedIds(filter.documented < 0 ? QList<int>() : QList<int>{filter.documented});
  m_verifiedFilter->setCheckedIds(filter.verified < 0 ? QList<int>() : QList<int>{filter.verified});
  m_loading = false;
  refresh();
}
