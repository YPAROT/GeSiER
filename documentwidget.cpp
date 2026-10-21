#include "documentwidget.h"
#include "documentcreationdialog.h"
#include "docxexportservice.h"
#include "imageeditdialog.h"
#include "producttreeservice.h"
#include "tabularservice.h"
#include "tabularexportdialog.h"

#include <QtWidgets>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <functional>

DocumentWidget::DocumentWidget(QWidget *parent) : QWidget(parent) {
  m_documents = new QListWidget;
  m_documents->setObjectName("documentList");
  m_documents->setWordWrap(true);
  m_documents->setTextElideMode(Qt::ElideNone);
  m_documents->setUniformItemSizes(false);
  m_documents->setResizeMode(QListView::Adjust);
  m_documents->setMaximumWidth(300);
  m_tree = new QTreeWidget;
  m_tree->setObjectName("documentStructureTree");
  m_tree->setHeaderLabels({"Structure du document"});
  m_tree->setDragDropMode(QAbstractItemView::InternalMove);
  m_tree->setDefaultDropAction(Qt::MoveAction);
  m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_reference = new QLineEdit;
  m_title = new QLineEdit;
  m_template = new QLineEdit;
  m_template->setObjectName("documentTemplatePath");
  m_secondary = new QLineEdit;
  m_description = new QTextEdit;
  m_metadata = new QTableWidget(0, 2);
  m_metadata->setHorizontalHeaderLabels({"Clé", "Valeur"});
  m_metadata->horizontalHeader()->setStretchLastSection(true);
  m_type = new QComboBox;
  m_pt = new QComboBox;
  auto *addDocument = new QPushButton("Nouveau document");
  auto *save = new QPushButton("Enregistrer");
  save->setObjectName("documentSaveButton");
  auto *addChapter = new QPushButton("Ajouter un chapitre");
  addChapter->setToolTip("Ajoute un chapitre au même niveau que la sélection");
  auto *addSubchapter = new QPushButton("Ajouter un sous-chapitre");
  auto *promote = new QPushButton("Niveau −");
  promote->setToolTip("Remonter le chapitre sélectionné d'un niveau");
  auto *demote = new QPushButton("Niveau +");
  demote->setToolTip("Placer le chapitre sélectionné sous le chapitre précédent");
  auto *addRequirement = new QPushButton("Ajouter une exigence");
  auto *addText = new QPushButton("Ajouter du texte");
  auto *addImage = new QPushButton("Ajouter une image");
  auto *preview = new QPushButton("Prévisualiser");
  auto *chooseTemplate = new QPushButton("Parcourir…");
  auto *validateTemplate = new QPushButton("Valider le template");
  auto *draft = new QPushButton("Exporter un draft DOCX");
  auto *publish = new QPushButton("Publier en DOCX");
  m_exportObsolete = new QCheckBox("Exporter les exigences obsolètes");
  m_exportObsolete->setObjectName("exportObsoleteRequirements");
  m_exportObsolete->setToolTip(
      "Les exigences obsolètes exportées apparaîtront en gris dans le document Word.");
  auto *history = new QPushButton("Historique des exports");
  auto *exportTable = new QPushButton("Exporter CSV/XLSX");
  auto *addMetadata = new QPushButton("+ Ajouter");
  addMetadata->setObjectName("addDocumentMetadataButton");
  addMetadata->setToolTip("Ajouter une métadonnée au document");
  addMetadata->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  auto *removeMetadata = new QPushButton("− Supprimer");
  removeMetadata->setObjectName("removeDocumentMetadataButton");
  removeMetadata->setToolTip("Supprimer la métadonnée sélectionnée");
  removeMetadata->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  auto *remove = new QPushButton("Retirer");
  auto *left = new QWidget;
  left->setObjectName("documentListPanel");
  left->setMaximumWidth(300);
  auto *leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->addWidget(addDocument);
  leftLayout->addWidget(m_documents);
  auto *properties = new QFormLayout;
  properties->addRow("Référence principale", m_reference);
  properties->addRow("Références secondaires (;)", m_secondary);
  properties->addRow("Titre", m_title);
  properties->addRow("Type", m_type);
  properties->addRow("Product Tree", m_pt);
  properties->addRow("Description", m_description);
  auto *metadataActions = new QHBoxLayout;
  metadataActions->addWidget(addMetadata);
  metadataActions->addWidget(removeMetadata);
  metadataActions->addStretch();
  auto *metadataBox = new QVBoxLayout;
  metadataBox->addLayout(metadataActions);
  metadataBox->addWidget(m_metadata);
  properties->addRow("Métadonnées", metadataBox);

  auto *structureGroup = new QGroupBox("Trame");
  structureGroup->setObjectName("documentStructureActions");
  structureGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *structureActions = new QHBoxLayout(structureGroup);
  structureActions->addWidget(addChapter);
  structureActions->addWidget(addSubchapter);
  structureActions->addWidget(promote);
  structureActions->addWidget(demote);
  structureActions->addStretch();
  auto *contentGroup = new QGroupBox("Contenu");
  contentGroup->setObjectName("documentContentActions");
  contentGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *contentActions = new QHBoxLayout(contentGroup);
  contentActions->addWidget(addRequirement);
  contentActions->addWidget(addText);
  contentActions->addWidget(addImage);
  contentActions->addWidget(remove);
  contentActions->addStretch();
  auto *editPage = new QWidget;
  editPage->setObjectName("documentEditActions");
  auto *editLayout = new QHBoxLayout(editPage);
  editLayout->setContentsMargins(6, 6, 6, 6);
  editLayout->addWidget(structureGroup);
  editLayout->addWidget(contentGroup);
  editLayout->addWidget(save, 0, Qt::AlignRight | Qt::AlignVCenter);

  auto *templateGroup = new QGroupBox("Template DOCX");
  templateGroup->setObjectName("documentTemplateGroup");
  templateGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *templateRow = new QHBoxLayout(templateGroup);
  templateRow->addWidget(m_template, 1);
  templateRow->addWidget(chooseTemplate);
  templateRow->addWidget(validateTemplate);
  auto *generationGroup = new QGroupBox("Génération");
  generationGroup->setObjectName("documentGenerationActions");
  generationGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *generationActions = new QHBoxLayout(generationGroup);
  generationActions->addWidget(preview);
  generationActions->addWidget(m_exportObsolete);
  generationActions->addStretch();
  generationActions->addWidget(draft);
  generationActions->addWidget(publish);
  auto *otherExportsGroup = new QGroupBox("Données et historique");
  otherExportsGroup->setObjectName("documentOtherExportActions");
  otherExportsGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *otherExports = new QHBoxLayout(otherExportsGroup);
  otherExports->addWidget(history);
  otherExports->addWidget(exportTable);
  otherExports->addStretch();
  auto *exportPage = new QWidget;
  exportPage->setObjectName("documentExportActions");
  auto *exportLayout = new QVBoxLayout(exportPage);
  exportLayout->setContentsMargins(6, 6, 6, 6);
  exportLayout->addWidget(templateGroup);
  exportLayout->addWidget(generationGroup);
  exportLayout->addWidget(otherExportsGroup);

  auto *actionTabs = new QTabWidget;
  actionTabs->setObjectName("documentActionTabs");
  actionTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  actionTabs->addTab(editPage, "Édition");
  actionTabs->addTab(exportPage, "Export");
  auto fitActionTabs = [actionTabs](int index) {
    QWidget *page = actionTabs->widget(index);
    if (!page)
      return;
    const int frameWidth = actionTabs->style()->pixelMetric(
        QStyle::PM_DefaultFrameWidth, nullptr, actionTabs);
    const int height = actionTabs->tabBar()->sizeHint().height() +
                       page->sizeHint().height() +
                       2 * frameWidth + 4;
    actionTabs->setFixedHeight(height);
  };
  connect(actionTabs, &QTabWidget::currentChanged, this, fitActionTabs);
  fitActionTabs(0);
  auto *right = new QWidget;
  auto *rightLayout = new QVBoxLayout(right);
  rightLayout->addLayout(properties);
  rightLayout->addWidget(actionTabs);
  rightLayout->addWidget(m_tree, 1);
  auto *splitter = new QSplitter;
  splitter->setObjectName("documentSplitter");
  splitter->addWidget(left);
  splitter->addWidget(right);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({300, 1200});
  splitter->restoreState(
      QSettings().value("Documents/splitter").toByteArray());
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(splitter);
  connect(splitter, &QSplitter::splitterMoved, this, [splitter] {
    QSettings().setValue("Documents/splitter", splitter->saveState());
  });
  connect(m_documents, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0)
      loadDocument(m_documents->item(row)->data(Qt::UserRole).toInt());
  });
  connect(exportTable, &QPushButton::clicked, this, [this] {
    const QStringList available{"Référence", "Titre", "Description", "Références secondaires"};
    TabularExportDialog options("documents-export", available, this);
    if (options.exec() != QDialog::Accepted) return;
    QString path = QFileDialog::getSaveFileName(this, "Exporter les documents",
                                                "documents.xlsx",
                                                "Classeur Excel (*.xlsx);;CSV (*.csv)");
    if (path.isEmpty()) return;
    const bool csv = path.endsWith(".csv", Qt::CaseInsensitive);
    if (!csv && !path.endsWith(".xlsx", Qt::CaseInsensitive)) path += ".xlsx";
    QList<QStringList> documents{options.outputHeaders()};
    for (const auto &document : m_service.documents()) {
      const QStringList complete{document.reference, document.title, document.description,
                                 document.secondaryReferences.join(" ; ")};
      QStringList selected;
      for (int column : options.columns()) selected << complete.value(column);
      documents << selected;
    }
    QList<QStringList> structure{{"Document", "Type", "Parent", "Position", "Contenu"}};
    for (const auto &document : m_service.documents())
      for (const auto &node : m_service.nodes(document.id))
        structure << QStringList{document.reference, node.type,
                                 QString::number(node.parentId), QString::number(node.position),
                                 node.type == "REQUIREMENT" ? node.requirementCode + " — " + node.requirementTitle
                                                            : node.type == "TEXT" ? node.textContent : node.title};
    QString error;
    const bool ok = csv ? TabularService::writeCsv(path, {options.sheetName(), documents}, ';', &error)
                        : TabularService::writeXlsx(path, {{options.sheetName(), documents},
                                                          {"Structure", structure}}, &error);
    if (ok) QMessageBox::information(this, "Export tabulaire", "Export terminé.");
    else QMessageBox::warning(this, "Export tabulaire", error);
  });
  connect(addDocument, &QPushButton::clicked, this, [this] {
    DocumentCreationDialog dialog(m_connection, this);
    if (dialog.exec() != QDialog::Accepted)
      return;
    const RequirementResult result =
        m_service.createDocument(dialog.record(), dialog.options());
    if (!result.success) {
      QMessageBox::warning(this, "Document", result.message);
      return;
    }
    m_current = result.id;
    refresh();
    openDocument(m_current);
    emit dataChanged();
  });
  connect(addMetadata, &QPushButton::clicked, this, [this] { m_metadata->insertRow(m_metadata->rowCount()); });
  connect(removeMetadata, &QPushButton::clicked, this, [this] {
    if (m_metadata->currentRow() >= 0) m_metadata->removeRow(m_metadata->currentRow());
  });
  connect(save, &QPushButton::clicked, this, [this] {
    if (m_current < 0) {
      QMessageBox::information(
          this, "Document",
          "Utilisez « Nouveau document » pour créer un document.");
      return;
    }
    DocumentRecord record;
    record.id = m_current;
    record.reference = m_reference->text();
    record.secondaryReferences = m_secondary->text().split(';', Qt::SkipEmptyParts);
    record.title = m_title->text();
    record.description = m_description->toPlainText();
    record.templatePath = m_template->text().trimmed();
    record.typeId = m_type->currentData().toInt();
    record.ptId = m_pt->currentData().toInt();
    for (int row=0; row<m_metadata->rowCount(); ++row) {
      const QString key=m_metadata->item(row,0)?m_metadata->item(row,0)->text().trimmed():QString();
      const QString value=m_metadata->item(row,1)?m_metadata->item(row,1)->text():QString();
      if(!key.isEmpty()) record.metadata[key]=value;
    }
    const RequirementResult result = m_service.saveDocument(record);
    if (!result.success) QMessageBox::warning(this, "Document", result.message);
    else { m_current = result.id; refresh(); openDocument(m_current); emit dataChanged(); }
  });
  connect(addChapter, &QPushButton::clicked, this, [this] {
    if (m_current < 0) return;
    bool ok = false;
    const QString title = QInputDialog::getText(this, "Chapitre", "Titre", QLineEdit::Normal, {}, &ok);
    if (ok) {
      const RequirementResult result =
          m_service.addChapter(m_current, selectedSiblingParent(), title);
      if (!result.success) QMessageBox::warning(this, "Document", result.message);
      else { loadTree(m_current); emit dataChanged(); }
    }
  });
  connect(addSubchapter, &QPushButton::clicked, this, [this] {
    if (m_current < 0) return;
    QTreeWidgetItem *selected = m_tree->currentItem();
    if (!selected || selected->data(0, Qt::UserRole + 2).toString() != "CHAPTER") {
      QMessageBox::information(this, "Document", "Sélectionnez le chapitre parent.");
      return;
    }
    bool ok = false;
    const QString title = QInputDialog::getText(
        this, "Sous-chapitre", "Titre", QLineEdit::Normal, {}, &ok);
    if (!ok) return;
    const RequirementResult result = m_service.addChapter(
        m_current, selected->data(0, Qt::UserRole).toInt(), title);
    if (!result.success) QMessageBox::warning(this, "Document", result.message);
    else { loadTree(m_current); emit dataChanged(); }
  });
  connect(addRequirement, &QPushButton::clicked, this, [this] {
    if (m_current < 0) return;
    QSqlQuery query("SELECT ID,CODE||' — '||TITLE FROM REQUIREMENT ORDER BY CODE", QSqlDatabase::database(m_connection, false));
    QStringList labels; QList<int> ids;
    while (query.next()) { ids << query.value(0).toInt(); labels << query.value(1).toString(); }
    bool ok = false;
    const QString choice = QInputDialog::getItem(this, "Ajouter une exigence", "Exigence", labels, 0, false, &ok);
    const int index = labels.indexOf(choice);
    if (ok && index >= 0) {
      const RequirementResult result = m_service.placeRequirement(m_current, selectedContainer(), ids[index]);
      if (!result.success) QMessageBox::warning(this, "Document", result.message);
      else { loadTree(m_current); emit dataChanged(); }
    }
  });
  connect(addText, &QPushButton::clicked, this, [this] {
    if(m_current<0)return;
    QDialog dialog(this);dialog.setWindowTitle("Texte riche");
    auto *editor=new QTextEdit;auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    auto *layout=new QVBoxLayout(&dialog);layout->addWidget(editor);layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted){const auto result=m_service.addText(m_current,selectedContainer(),editor->toHtml());if(!result.success)QMessageBox::warning(this,"Document",result.message);else{loadTree(m_current);emit dataChanged();}}
  });
  connect(addImage, &QPushButton::clicked, this, [this] {
    if(m_current<0)return;
    const QString path=QFileDialog::getOpenFileName(this,"Ajouter une image",{},"Images (*.png *.jpg *.jpeg *.bmp)");if(path.isEmpty())return;
    QFile file(path);if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(this,"Document",file.errorString());return;}
    bool ok=false;const QString legend=QInputDialog::getText(this,"Image","Légende",QLineEdit::Normal,{},&ok);if(!ok)return;
    const auto result=m_service.addImage(m_current,selectedContainer(),file.readAll(),legend);if(!result.success)QMessageBox::warning(this,"Document",result.message);else{loadTree(m_current);emit dataChanged();}
  });
  connect(promote, &QPushButton::clicked, this, [this] {
    QTreeWidgetItem *item=m_tree->currentItem();
    if(!item || !item->parent()) return;
    QTreeWidgetItem *parent=item->parent();
    QTreeWidgetItem *grandParent=parent->parent();
    const int newParent=grandParent?grandParent->data(0,Qt::UserRole).toInt():-1;
    const int parentPosition=grandParent?grandParent->indexOfChild(parent):m_tree->indexOfTopLevelItem(parent);
    const RequirementResult result=m_service.moveNode(item->data(0,Qt::UserRole).toInt(),newParent,parentPosition+1);
    if(!result.success)QMessageBox::warning(this,"Document",result.message);
    loadTree(m_current); emit dataChanged();
  });
  connect(demote, &QPushButton::clicked, this, [this] {
    QTreeWidgetItem *item=m_tree->currentItem(); if(!item)return;
    QTreeWidgetItem *parent=item->parent();
    const int index=parent?parent->indexOfChild(item):m_tree->indexOfTopLevelItem(item);
    if(index<=0)return;
    QTreeWidgetItem *previous=parent?parent->child(index-1):m_tree->topLevelItem(index-1);
    if(previous->data(0,Qt::UserRole+2).toString()!="CHAPTER"){
      QMessageBox::information(this,"Document","L'élément précédent doit être un chapitre.");return;
    }
    const RequirementResult result=m_service.moveNode(item->data(0,Qt::UserRole).toInt(),previous->data(0,Qt::UserRole).toInt(),previous->childCount());
    if(!result.success)QMessageBox::warning(this,"Document",result.message);
    loadTree(m_current); emit dataChanged();
  });
  connect(preview, &QPushButton::clicked, this, [this] {
    if(m_current<0)return;
    QDialog dialog(this);dialog.setWindowTitle("Prévisualisation — "+m_title->text());dialog.resize(800,650);auto *browser=new QTextBrowser;browser->setHtml(m_service.previewHtml(m_current));auto *layout=new QVBoxLayout(&dialog);layout->addWidget(browser);dialog.exec();
  });
  connect(chooseTemplate,&QPushButton::clicked,this,[this]{const QString p=QFileDialog::getOpenFileName(this,"Choisir un template DOCX",m_template->text(),"Documents Word (*.docx)");if(!p.isEmpty())m_template->setText(p);});
  connect(validateTemplate,&QPushButton::clicked,this,[this]{DocxExportService s(m_connection);const auto v=s.validateTemplate(m_template->text(),m_current);QString message=v.valid?"Template valide.":v.errors.join("\n");if(!v.warnings.isEmpty())message+="\n\nAvertissements :\n"+v.warnings.join("\n");if(v.valid)QMessageBox::information(this,"Template DOCX",message);else QMessageBox::warning(this,"Template DOCX",message);});
  auto runExport=[this](bool publication){if(m_current<0)return;DocxExportService service(m_connection);const auto validation=service.validateTemplate(m_template->text(),m_current);if(!validation.valid){QMessageBox::warning(this,"Export DOCX",validation.errors.join("\n"));return;}if(!validation.warnings.isEmpty()&&QMessageBox::warning(this,"Template DOCX",validation.warnings.join("\n")+"\n\nContinuer ?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;DocxExportRequest request;request.documentId=m_current;request.publication=publication;request.includeObsoleteRequirements=m_exportObsolete->isChecked();request.templatePath=m_template->text();request.author=qEnvironmentVariable("USERNAME");QSettings settings;const QStringList relationModes={tr("Libellés traduits"),tr("Codes techniques")};const int savedMode=settings.value("documents/docxRelationPresentation",0).toInt();bool relationModeAccepted=false;const QString relationMode=QInputDialog::getItem(this,tr("Export DOCX"),tr("Présentation des relations"),relationModes,qBound(0,savedMode,1),false,&relationModeAccepted);if(!relationModeAccepted)return;const int relationModeIndex=relationModes.indexOf(relationMode);request.relationPresentation=relationModeIndex==1?DocxExportRequest::RelationPresentation::TechnicalCodes:DocxExportRequest::RelationPresentation::TranslatedLabels;settings.setValue("documents/docxRelationPresentation",relationModeIndex);if(publication){bool ok=false;request.version=QInputDialog::getText(this,"Publication","Version",QLineEdit::Normal,{},&ok);if(!ok)return;request.title=QInputDialog::getText(this,"Publication","Titre publié",QLineEdit::Normal,m_title->text(),&ok);if(!ok)return;}else{bool ok=false;request.draftLabel=QInputDialog::getText(this,"Draft","Libellé provisoire",QLineEdit::Normal,m_title->text(),&ok);if(!ok)return;}request.outputPath=QFileDialog::getSaveFileName(this,publication?"Publier le document":"Exporter le draft",{},"Documents Word (*.docx)");if(request.outputPath.isEmpty())return;if(!request.outputPath.endsWith(".docx",Qt::CaseInsensitive))request.outputPath+=".docx";const auto result=service.exportDocument(request);if(!result.success)QMessageBox::critical(this,"Export DOCX",result.message);else QMessageBox::information(this,"Export DOCX",result.message);};
  connect(draft,&QPushButton::clicked,this,[runExport]{runExport(false);});connect(publish,&QPushButton::clicked,this,[runExport]{runExport(true);});
  connect(history,&QPushButton::clicked,this,[this]{if(m_current<0)return;DocxExportService service(m_connection);const auto rows=service.history(m_current);QDialog dialog(this);dialog.setWindowTitle("Historique des exports");dialog.resize(900,400);auto *table=new QTableWidget(rows.size(),7);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setHorizontalHeaderLabels({"Type","Version / libellé","Auteur","Date","SHA-256","GED","Fichier"});for(int i=0;i<rows.size();++i){const auto&r=rows[i];const QStringList values={r.kind,r.version.isEmpty()?r.title:r.version,r.author,r.exportedAt,r.sha256,r.gedReference,r.filePath};for(int c=0;c<values.size();++c)table->setItem(i,c,new QTableWidgetItem(values[c]));table->item(i,0)->setData(Qt::UserRole,r.id);table->item(i,0)->setData(Qt::UserRole+1,r.kind);}table->horizontalHeader()->setStretchLastSection(true);auto *ged=new QPushButton("Renseigner la GED");auto *verify=new QPushButton("Vérifier l'empreinte");auto *actions=new QHBoxLayout;actions->addWidget(ged);actions->addWidget(verify);actions->addStretch();auto *layout=new QVBoxLayout(&dialog);layout->addWidget(table);layout->addLayout(actions);connect(ged,&QPushButton::clicked,&dialog,[&]{const int row=table->currentRow();if(row<0||table->item(row,0)->data(Qt::UserRole+1).toString()!="PUBLICATION"){QMessageBox::warning(&dialog,"GED","Sélectionnez une publication.");return;}bool ok=false;const QString ref=QInputDialog::getText(&dialog,"GED","Identifiant GED",QLineEdit::Normal,{},&ok);if(!ok)return;const QString link=QInputDialog::getText(&dialog,"GED","Lien GED",QLineEdit::Normal,{},&ok);if(!ok)return;const auto result=service.setGedInformation(table->item(row,0)->data(Qt::UserRole).toInt(),ref,link);if(result.success)table->item(row,5)->setText(ref);else QMessageBox::warning(&dialog,"GED",result.message);});connect(verify,&QPushButton::clicked,&dialog,[&]{const int row=table->currentRow();if(row<0)return;QString error;const bool ok=service.verifyHash(table->item(row,0)->data(Qt::UserRole).toInt(),&error);if(ok)QMessageBox::information(&dialog,"Empreinte","Empreinte SHA-256 conforme.");else QMessageBox::warning(&dialog,"Empreinte",error);});dialog.exec();});
  connect(remove, &QPushButton::clicked, this, [this] {
    if (!m_tree->currentItem()) return;
    const int id = m_tree->currentItem()->data(0, Qt::UserRole).toInt();
    if (QMessageBox::question(this, "Document", "Retirer cet élément du document ?") == QMessageBox::Yes) {
      const RequirementResult result = m_service.removeNode(id);
      if (!result.success) QMessageBox::warning(this, "Document", result.message);
      else { loadTree(m_current); emit dataChanged(); }
    }
  });
  connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
    const QString type=item->data(0,Qt::UserRole+2).toString();
    const int requirementId = item->data(0, Qt::UserRole + 1).toInt();
    if (requirementId > 0) { emit openRequirement(requirementId); return; }
    if(type=="TEXT") { QDialog dialog(this);dialog.setWindowTitle("Modifier le texte");auto *editor=new QTextEdit;editor->setHtml(item->data(0,Qt::UserRole+3).toString());auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);auto *layout=new QVBoxLayout(&dialog);layout->addWidget(editor);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()==QDialog::Accepted){const auto result=m_service.updateText(item->data(0,Qt::UserRole).toInt(),editor->toHtml());if(!result.success)QMessageBox::warning(this,"Document",result.message);else loadTree(m_current);} }
    if(type=="IMAGE") {
      const int nodeId=item->data(0,Qt::UserRole).toInt();
      for(const auto&node:m_service.nodes(m_current)) if(node.id==nodeId) {
        ImageEditDialog dialog(node.imageData,node.imageLegend,this);
        const int action=dialog.exec();
        if(action==ImageEditDialog::DeleteRequested) {
          const auto result=m_service.removeNode(nodeId);
          if(!result.success)QMessageBox::warning(this,"Document",result.message);
          else { loadTree(m_current); emit dataChanged(); }
        } else if(action==QDialog::Accepted) {
          const auto result=m_service.updateImage(nodeId,dialog.imageData(),dialog.legend());
          if(!result.success)QMessageBox::warning(this,"Document",result.message);
          else { loadTree(m_current); emit dataChanged(); }
        }
        break;
      }
    }
  });
  connect(m_tree, &QTreeWidget::itemChanged, this,
          [this](QTreeWidgetItem *item) {
            if (item->data(0, Qt::UserRole + 2).toString() == "CHAPTER")
              m_service.renameChapter(item->data(0, Qt::UserRole).toInt(),
                                      item->text(0));
          });
  connect(m_tree->model(), &QAbstractItemModel::rowsMoved, this,
          [this](const QModelIndex &sourceParent, int sourceStart,
                 int sourceEnd, const QModelIndex &destinationParent,
                 int destinationRow) {
    const int count = sourceEnd - sourceStart + 1;
    int firstDestinationRow = destinationRow;
    if (sourceParent == destinationParent && destinationRow > sourceEnd)
      firstDestinationRow -= count;

    const int parentId = destinationParent.isValid()
                             ? destinationParent.data(Qt::UserRole).toInt()
                             : -1;
    for (int offset = 0; offset < count; ++offset) {
      const QModelIndex moved = m_tree->model()->index(
          firstDestinationRow + offset, 0, destinationParent);
      const int nodeId = moved.data(Qt::UserRole).toInt();
      const RequirementResult result =
          m_service.moveNode(nodeId, parentId, firstDestinationRow + offset);
      if (!result.success) {
        QMessageBox::warning(
            this, "Document",
            "Le déplacement n'a pas pu être enregistré :\n" + result.message);
        loadTree(m_current);
        return;
      }
    }
    emit dataChanged();
  });
}

void DocumentWidget::setConnectionName(const QString &connectionName) {
  m_connection = connectionName;
  m_service = DocumentService(connectionName);
  refresh();
}

void DocumentWidget::releaseDatabase() {
  m_documents->clear();
  m_tree->clear();
  m_reference->clear();
  m_secondary->clear();
  m_title->clear();
  m_description->clear();
  m_template->clear();
  m_metadata->setRowCount(0);
  m_type->clear();
  m_pt->clear();
  m_connection.clear();
  m_current = -1;
}

void DocumentWidget::refresh() {
  const int selectedDocument = m_current;
  m_documents->clear(); m_type->clear(); m_pt->clear();
  if (m_connection.isEmpty() || !QSqlDatabase::database(m_connection, false).isOpen()) return;
  QSqlQuery types("SELECT ID,TYPE FROM DOC_TYPE ORDER BY ID", QSqlDatabase::database(m_connection, false));
  while (types.next()) m_type->addItem(types.value(1).toString(), types.value(0));
  ProductTreeService tree(m_connection);
  QSqlQuery pts("SELECT ID FROM PT WHERE ARCHIVED=0 ORDER BY POSITION,SEGMENT", QSqlDatabase::database(m_connection, false));
  while (pts.next()) m_pt->addItem(tree.fullCode(pts.value(0).toInt()), pts.value(0));
  for (const DocumentRecord &record : m_service.documents()) {
    auto *item =
        new QListWidgetItem(record.reference + "\n" + record.title);
    item->setData(Qt::UserRole, record.id);
    item->setToolTip({});
    m_documents->addItem(item);
  }
  if (selectedDocument >= 0)
    openDocument(selectedDocument);
  if (m_documents->currentRow() < 0) {
    m_current = -1;
    m_reference->clear(); m_secondary->clear(); m_title->clear();
    m_description->clear(); m_template->clear();
    m_metadata->setRowCount(0); m_tree->clear();
  }
}

void DocumentWidget::openDocument(int id) {
  for (int row = 0; row < m_documents->count(); ++row)
    if (m_documents->item(row)->data(Qt::UserRole).toInt() == id) {
      m_documents->setCurrentRow(row); return;
    }
}

void DocumentWidget::loadDocument(int id) {
  m_current = id;
  const DocumentRecord record = m_service.document(id);
  m_reference->setText(record.reference); m_title->setText(record.title);
  m_secondary->setText(record.secondaryReferences.join("; "));
  m_description->setPlainText(record.description);
  m_template->setText(record.templatePath);
  m_metadata->setRowCount(0);
  for(auto it=record.metadata.cbegin();it!=record.metadata.cend();++it){const int row=m_metadata->rowCount();m_metadata->insertRow(row);m_metadata->setItem(row,0,new QTableWidgetItem(it.key()));m_metadata->setItem(row,1,new QTableWidgetItem(it.value()));}
  m_type->setCurrentIndex(m_type->findData(record.typeId));
  m_pt->setCurrentIndex(m_pt->findData(record.ptId));
  loadTree(id);
}

void DocumentWidget::loadTree(int id) {
  const QSignalBlocker blocker(m_tree);
  m_tree->clear();
  QMap<int, QTreeWidgetItem *> items;
  const QList<DocumentNodeRecord> nodes = m_service.nodes(id);
  for (const auto &node : nodes) {
    const bool isRequirement = node.type == "REQUIREMENT";
    const QString requirementLabel =
        node.requirementTitle.isEmpty()
            ? node.requirementCode
            : node.requirementCode + " — " + node.requirementTitle;
    QString label=node.title;
    if(isRequirement) label=requirementLabel+(node.requirementObsolete?" [Obsolete]":"");
    else if(node.type=="TEXT") label="Texte — "+QTextDocumentFragment::fromHtml(node.textContent).toPlainText().simplified().left(60);
    else if(node.type=="IMAGE") label="Image — "+node.imageLegend;
    auto *item = new QTreeWidgetItem({label});
    item->setData(0, Qt::UserRole, node.id);
    item->setData(0, Qt::UserRole + 1, node.requirementId);
    item->setData(0, Qt::UserRole + 2, node.type);
    item->setData(0, Qt::UserRole + 3, node.textContent);
    if (isRequirement) {
      item->setToolTip(0, node.requirementCode + "\n" + node.requirementTitle);
      item->setForeground(0, node.requirementObsolete
                                 ? QBrush(QColor("#808080"))
                                 : palette().brush(QPalette::Link));
    } else if(node.type == "CHAPTER") {
      QFont chapterFont = item->font(0);
      chapterFont.setBold(true);
      item->setFont(0, chapterFont);
      item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsDropEnabled);
    }
    if (node.type != "CHAPTER")
      item->setFlags((item->flags() | Qt::ItemIsDragEnabled) & ~Qt::ItemIsDropEnabled);
    items[node.id] = item;
  }
  for (const auto &node : nodes) {
    if (items.contains(node.parentId)) items[node.parentId]->addChild(items[node.id]);
    else m_tree->addTopLevelItem(items[node.id]);
  }
  m_tree->expandAll();
}

int DocumentWidget::selectedContainer() const {
  if (!m_tree->currentItem()) return -1;
  return m_tree->currentItem()->data(0, Qt::UserRole + 1).toInt() > 0
             ? (m_tree->currentItem()->parent() ? m_tree->currentItem()->parent()->data(0, Qt::UserRole).toInt() : -1)
             : m_tree->currentItem()->data(0, Qt::UserRole).toInt();
}

int DocumentWidget::selectedSiblingParent() const {
  QTreeWidgetItem *item=m_tree->currentItem();
  if(!item || !item->parent()) return -1;
  return item->parent()->data(0,Qt::UserRole).toInt();
}
