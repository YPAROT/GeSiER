#include "documentwidget.h"
#include "docxexportservice.h"
#include "producttreeservice.h"

#include <QtWidgets>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <functional>

DocumentWidget::DocumentWidget(QWidget *parent) : QWidget(parent) {
  m_documents = new QListWidget;
  m_tree = new QTreeWidget;
  m_tree->setHeaderLabels({"Structure du document"});
  m_tree->setDragDropMode(QAbstractItemView::InternalMove);
  m_tree->setDefaultDropAction(Qt::MoveAction);
  m_reference = new QLineEdit;
  m_title = new QLineEdit;
  m_template = new QLineEdit;
  m_secondary = new QLineEdit;
  m_description = new QTextEdit;
  m_metadata = new QTableWidget(0, 2);
  m_metadata->setHorizontalHeaderLabels({"Clé", "Valeur"});
  m_metadata->horizontalHeader()->setStretchLastSection(true);
  m_type = new QComboBox;
  m_pt = new QComboBox;
  auto *addDocument = new QPushButton("Nouveau document");
  auto *save = new QPushButton("Enregistrer");
  auto *addChapter = new QPushButton("Ajouter un chapitre");
  auto *addRequirement = new QPushButton("Ajouter une exigence");
  auto *addText = new QPushButton("Ajouter du texte");
  auto *addImage = new QPushButton("Ajouter une image");
  auto *preview = new QPushButton("Prévisualiser");
  auto *chooseTemplate = new QPushButton("Parcourir…");
  auto *validateTemplate = new QPushButton("Valider le template");
  auto *draft = new QPushButton("Exporter un draft DOCX");
  auto *publish = new QPushButton("Publier en DOCX");
  auto *history = new QPushButton("Historique des exports");
  auto *addMetadata = new QPushButton("+ Métadonnée");
  auto *removeMetadata = new QPushButton("− Métadonnée");
  auto *remove = new QPushButton("Retirer");
  auto *left = new QWidget;
  auto *leftLayout = new QVBoxLayout(left);
  leftLayout->addWidget(addDocument);
  leftLayout->addWidget(m_documents);
  auto *properties = new QFormLayout;
  properties->addRow("Référence principale", m_reference);
  properties->addRow("Références secondaires (;)", m_secondary);
  properties->addRow("Titre", m_title);
  properties->addRow("Type", m_type);
  properties->addRow("Product Tree", m_pt);
  properties->addRow("Description", m_description);
  auto *templateRow=new QHBoxLayout;templateRow->addWidget(m_template);templateRow->addWidget(chooseTemplate);templateRow->addWidget(validateTemplate);
  properties->addRow("Template DOCX",templateRow);
  auto *metadataActions = new QHBoxLayout;
  metadataActions->addWidget(addMetadata); metadataActions->addWidget(removeMetadata);
  auto *metadataBox = new QVBoxLayout; metadataBox->addWidget(m_metadata); metadataBox->addLayout(metadataActions);
  properties->addRow("Métadonnées", metadataBox);
  auto *treeActions = new QHBoxLayout;
  treeActions->addWidget(addChapter);
  treeActions->addWidget(addRequirement);
  treeActions->addWidget(addText);
  treeActions->addWidget(addImage);
  treeActions->addWidget(remove);
  treeActions->addStretch();
  treeActions->addWidget(preview);
  treeActions->addWidget(draft);
  treeActions->addWidget(publish);
  treeActions->addWidget(history);
  treeActions->addWidget(save);
  auto *right = new QWidget;
  auto *rightLayout = new QVBoxLayout(right);
  rightLayout->addLayout(properties);
  rightLayout->addLayout(treeActions);
  rightLayout->addWidget(m_tree);
  auto *splitter = new QSplitter;
  splitter->addWidget(left);
  splitter->addWidget(right);
  splitter->setStretchFactor(1, 1);
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(splitter);
  connect(m_documents, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0)
      loadDocument(m_documents->item(row)->data(Qt::UserRole).toInt());
  });
  connect(addDocument, &QPushButton::clicked, this, [this] {
    m_current = -1;
    m_reference->clear(); m_secondary->clear(); m_title->clear();
    m_description->clear(); m_template->clear(); m_metadata->setRowCount(0); m_tree->clear();
  });
  connect(addMetadata, &QPushButton::clicked, this, [this] { m_metadata->insertRow(m_metadata->rowCount()); });
  connect(removeMetadata, &QPushButton::clicked, this, [this] {
    if (m_metadata->currentRow() >= 0) m_metadata->removeRow(m_metadata->currentRow());
  });
  connect(save, &QPushButton::clicked, this, [this] {
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
    if (ok && m_service.addChapter(m_current, selectedParent(), title).success) {
      loadTree(m_current); emit dataChanged();
    }
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
      const RequirementResult result = m_service.placeRequirement(m_current, selectedParent(), ids[index]);
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
    if(dialog.exec()==QDialog::Accepted){const auto result=m_service.addText(m_current,selectedParent(),editor->toHtml());if(!result.success)QMessageBox::warning(this,"Document",result.message);else{loadTree(m_current);emit dataChanged();}}
  });
  connect(addImage, &QPushButton::clicked, this, [this] {
    if(m_current<0)return;
    const QString path=QFileDialog::getOpenFileName(this,"Ajouter une image",{},"Images (*.png *.jpg *.jpeg *.bmp)");if(path.isEmpty())return;
    QFile file(path);if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(this,"Document",file.errorString());return;}
    bool ok=false;const QString legend=QInputDialog::getText(this,"Image","Légende",QLineEdit::Normal,{},&ok);if(!ok)return;
    const auto result=m_service.addImage(m_current,selectedParent(),file.readAll(),legend);if(!result.success)QMessageBox::warning(this,"Document",result.message);else{loadTree(m_current);emit dataChanged();}
  });
  connect(preview, &QPushButton::clicked, this, [this] {
    if(m_current<0)return;
    QDialog dialog(this);dialog.setWindowTitle("Prévisualisation — "+m_title->text());dialog.resize(800,650);auto *browser=new QTextBrowser;browser->setHtml(m_service.previewHtml(m_current));auto *layout=new QVBoxLayout(&dialog);layout->addWidget(browser);dialog.exec();
  });
  connect(chooseTemplate,&QPushButton::clicked,this,[this]{const QString p=QFileDialog::getOpenFileName(this,"Choisir un template DOCX",m_template->text(),"Documents Word (*.docx)");if(!p.isEmpty())m_template->setText(p);});
  connect(validateTemplate,&QPushButton::clicked,this,[this]{DocxExportService s(m_connection);const auto v=s.validateTemplate(m_template->text());QString message=v.valid?"Template valide.":v.errors.join("\n");if(!v.warnings.isEmpty())message+="\n\nAvertissements :\n"+v.warnings.join("\n");if(v.valid)QMessageBox::information(this,"Template DOCX",message);else QMessageBox::warning(this,"Template DOCX",message);});
  auto runExport=[this](bool publication){if(m_current<0)return;DocxExportService service(m_connection);const auto validation=service.validateTemplate(m_template->text());if(!validation.valid){QMessageBox::warning(this,"Export DOCX",validation.errors.join("\n"));return;}if(!validation.warnings.isEmpty()&&QMessageBox::warning(this,"Template DOCX",validation.warnings.join("\n")+"\n\nContinuer ?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;DocxExportRequest request;request.documentId=m_current;request.publication=publication;request.templatePath=m_template->text();request.author=qEnvironmentVariable("USERNAME");if(publication){bool ok=false;request.version=QInputDialog::getText(this,"Publication","Version",QLineEdit::Normal,{},&ok);if(!ok)return;request.title=QInputDialog::getText(this,"Publication","Titre publié",QLineEdit::Normal,m_title->text(),&ok);if(!ok)return;}else{bool ok=false;request.draftLabel=QInputDialog::getText(this,"Draft","Libellé provisoire",QLineEdit::Normal,m_title->text(),&ok);if(!ok)return;}request.outputPath=QFileDialog::getSaveFileName(this,publication?"Publier le document":"Exporter le draft",{},"Documents Word (*.docx)");if(request.outputPath.isEmpty())return;if(!request.outputPath.endsWith(".docx",Qt::CaseInsensitive))request.outputPath+=".docx";const auto result=service.exportDocument(request);if(!result.success)QMessageBox::critical(this,"Export DOCX",result.message);else QMessageBox::information(this,"Export DOCX",result.message);};
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
    if(type=="IMAGE") { const int nodeId=item->data(0,Qt::UserRole).toInt();for(const auto&node:m_service.nodes(m_current))if(node.id==nodeId){bool ok=false;const QString legend=QInputDialog::getText(this,"Modifier l'image","Légende",QLineEdit::Normal,node.imageLegend,&ok);if(!ok)return;QByteArray data=node.imageData;if(QMessageBox::question(this,"Modifier l'image","Remplacer le fichier image ?")==QMessageBox::Yes){const QString path=QFileDialog::getOpenFileName(this,"Remplacer l'image",{},"Images (*.png *.jpg *.jpeg *.bmp)");QFile file(path);if(!path.isEmpty()&&file.open(QIODevice::ReadOnly))data=file.readAll();}const auto result=m_service.updateImage(nodeId,data,legend);if(!result.success)QMessageBox::warning(this,"Document",result.message);else loadTree(m_current);break;} }
  });
  connect(m_tree, &QTreeWidget::itemChanged, this,
          [this](QTreeWidgetItem *item) {
            if (item->data(0, Qt::UserRole + 2).toString() == "CHAPTER")
              m_service.renameChapter(item->data(0, Qt::UserRole).toInt(),
                                      item->text(0));
          });
  connect(m_tree->model(), &QAbstractItemModel::rowsMoved, this, [this] {
    std::function<void(QTreeWidgetItem *, int)> persist =
        [this, &persist](QTreeWidgetItem *parent, int parentId) {
          const int count = parent ? parent->childCount() : m_tree->topLevelItemCount();
          for (int position = 0; position < count; ++position) {
            QTreeWidgetItem *item = parent ? parent->child(position)
                                           : m_tree->topLevelItem(position);
            const int id = item->data(0, Qt::UserRole).toInt();
            m_service.moveNode(id, parentId, position);
            persist(item, id);
          }
        };
    persist(nullptr, -1);
    emit dataChanged();
  });
}

void DocumentWidget::setConnectionName(const QString &connectionName) {
  m_connection = connectionName;
  m_service = DocumentService(connectionName);
  refresh();
}

void DocumentWidget::releaseDatabase() {
  m_documents->clear(); m_tree->clear(); m_connection.clear(); m_current = -1;
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
    auto *item = new QListWidgetItem(record.reference + " — " + record.title);
    item->setData(Qt::UserRole, record.id);
    m_documents->addItem(item);
  }
  if (selectedDocument >= 0)
    openDocument(selectedDocument);
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
    if(isRequirement) label=requirementLabel;
    else if(node.type=="TEXT") label="Texte — "+QTextDocumentFragment::fromHtml(node.textContent).toPlainText().simplified().left(60);
    else if(node.type=="IMAGE") label="Image — "+node.imageLegend;
    auto *item = new QTreeWidgetItem({label});
    item->setData(0, Qt::UserRole, node.id);
    item->setData(0, Qt::UserRole + 1, node.requirementId);
    item->setData(0, Qt::UserRole + 2, node.type);
    item->setData(0, Qt::UserRole + 3, node.textContent);
    if (isRequirement) {
      item->setToolTip(0, node.requirementCode + "\n" + node.requirementTitle);
      item->setForeground(0, palette().brush(QPalette::Link));
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

int DocumentWidget::selectedParent() const {
  if (!m_tree->currentItem()) return -1;
  return m_tree->currentItem()->data(0, Qt::UserRole + 1).toInt() > 0
             ? (m_tree->currentItem()->parent() ? m_tree->currentItem()->parent()->data(0, Qt::UserRole).toInt() : -1)
             : m_tree->currentItem()->data(0, Qt::UserRole).toInt();
}
