#include "applicabilitywidget.h"
#include "historyservice.h"
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtWidgets>

namespace {
void fill(QComboBox*c,QSqlDatabase db,const QString&sql,const QString&all){c->blockSignals(true);c->clear();c->addItem(all,-1);QSqlQuery q(sql,db);while(q.next())c->addItem(q.value(1).toString(),q.value(0));c->blockSignals(false);}
}
ApplicabilityWidget::ApplicabilityWidget(QWidget *parent):QWidget(parent){
  auto tabs=new QTabWidget;
  auto configPage=new QWidget;auto cv=new QVBoxLayout(configPage);auto ca=new QHBoxLayout;
  auto add=new QPushButton("Nouvelle");auto edit=new QPushButton("Modifier");m_archive=new QPushButton("Archiver / restaurer");ca->addWidget(add);ca->addWidget(edit);ca->addWidget(m_archive);ca->addStretch();
  m_configurations=new QTableWidget(0,7);m_configurations->setHorizontalHeaderLabels({"Code","Libellé","Description","Ordre","État","Utilisations","Mise à jour"});m_configurations->setSelectionBehavior(QAbstractItemView::SelectRows);m_configurations->setEditTriggers(QAbstractItemView::NoEditTriggers);m_configurations->horizontalHeader()->setStretchLastSection(true);
  m_history=new QTableWidget(0,3);m_history->setHorizontalHeaderLabels({"Date","Événement","Modifications"});m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);m_history->horizontalHeader()->setStretchLastSection(true);
  cv->addLayout(ca);cv->addWidget(m_configurations,2);cv->addWidget(new QLabel("Historique de la configuration sélectionnée"));cv->addWidget(m_history,1);tabs->addTab(configPage,"Configurations");
  auto matrixPage=new QWidget;auto mv=new QVBoxLayout(matrixPage);auto filters=new QHBoxLayout;m_pt=new QComboBox;m_document=new QComboBox;m_status=new QComboBox;m_type=new QComboBox;m_obsolete=new QCheckBox("Inclure les exigences obsolètes");m_archivedConfigurations=new QCheckBox("Afficher les configurations archivées");m_notApplicableRequirements=new QCheckBox("Afficher les exigences non applicables");for(auto p:QList<QPair<QString,QWidget*>>{{"Branche PT",m_pt},{"Document",m_document},{"Statut",m_status},{"Type",m_type}}){filters->addWidget(new QLabel(p.first));filters->addWidget(p.second);}filters->addWidget(m_obsolete);filters->addWidget(m_notApplicableRequirements);filters->addWidget(m_archivedConfigurations);filters->addStretch();auto exportButton=new QPushButton("Exporter XLSX…");filters->addWidget(exportButton);
  m_rate=new QLabel;m_rate->setTextInteractionFlags(Qt::TextSelectableByMouse);auto*legend=new QLabel("<span style='background:#F3F0EC;color:#6B625A;padding:2px'>OBS</span> Obsolète · <span style='background:#EAF2F8;color:#40566B;padding:2px'>N/A</span> Non applicable au projet · <span style='background:#D9DEE5;color:#667085;padding:2px'>ARCH</span> Configuration archivée · <span style='background:#FFF2CC;padding:2px'>Jaune</span> Exception PT");legend->setTextFormat(Qt::RichText);m_matrix=new QTableWidget;m_matrix->setSelectionBehavior(QAbstractItemView::SelectRows);m_matrix->setSelectionMode(QAbstractItemView::SingleSelection);m_matrix->setEditTriggers(QAbstractItemView::NoEditTriggers);m_matrix->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);m_matrix->verticalHeader()->hide();m_matrix->verticalHeader()->setDefaultSectionSize(24);mv->addLayout(filters);mv->addWidget(m_rate);mv->addWidget(legend);mv->addWidget(m_matrix);tabs->addTab(matrixPage,"Matrice exigences × configurations");auto root=new QVBoxLayout(this);root->addWidget(tabs);
  m_rate->setTextFormat(Qt::RichText);m_rate->setTextInteractionFlags(Qt::TextBrowserInteraction);m_rate->setOpenExternalLinks(false);
  connect(m_rate,&QLabel::linkActivated,this,[this]{m_matrix->setFocus();m_matrix->scrollToTop();});
  connect(add,&QPushButton::clicked,this,[this]{editConfiguration();});connect(edit,&QPushButton::clicked,this,[this]{int r=m_configurations->currentRow();if(r>=0)editConfiguration(m_configurations->item(r,0)->data(Qt::UserRole).toInt());});connect(m_configurations,&QTableWidget::cellDoubleClicked,this,[this](int r){editConfiguration(m_configurations->item(r,0)->data(Qt::UserRole).toInt());});
  connect(m_archive,&QPushButton::clicked,this,[this]{int r=m_configurations->currentRow();if(r<0)return;const auto c=m_service.configuration(m_configurations->item(r,0)->data(Qt::UserRole).toInt());const auto result=m_service.setConfigurationActive(c.id,!c.active);if(!result.success)QMessageBox::warning(this,"Configuration",result.message);else{refresh();emit dataChanged();}});
  connect(m_configurations,&QTableWidget::itemSelectionChanged,this,[this]{m_history->setRowCount(0);int r=m_configurations->currentRow();if(r<0)return;QSqlQuery q(QSqlDatabase::database(m_connection));q.prepare("SELECT E.EVENT_TIME,E.EVENT_TYPE,COALESCE(E.BEFORE_JSON,''),COALESCE(E.AFTER_JSON,'') FROM EVENT_LOG E WHERE EXISTS(SELECT 1 FROM EVENT_SUBJECT S WHERE S.EVENT_ID=E.ID AND S.OBJECT_TYPE='CONFIGURATION' AND S.OBJECT_ID=?) ORDER BY E.ID DESC");q.addBindValue(m_configurations->item(r,0)->data(Qt::UserRole));if(q.exec())while(q.next()){int row=m_history->rowCount();m_history->insertRow(row);m_history->setItem(row,0,new QTableWidgetItem(q.value(0).toString()));m_history->setItem(row,1,new QTableWidgetItem(q.value(1).toString()));auto*label=new QLabel(HistoryService::formatDiff(q.value(2).toString(),q.value(3).toString()));label->setTextFormat(Qt::RichText);label->setWordWrap(true);label->setTextInteractionFlags(Qt::TextSelectableByMouse);label->setToolTip("Avant : "+q.value(2).toString()+"\nAprès : "+q.value(3).toString());m_history->setCellWidget(row,2,label);m_history->setRowHeight(row,qMax(36,label->sizeHint().height()+6));}});
  for(auto*c:{m_pt,m_document,m_status,m_type})
    connect(c,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&ApplicabilityWidget::refreshMatrix);
  connect(m_obsolete,&QCheckBox::toggled,this,&ApplicabilityWidget::refreshMatrix);
  connect(m_notApplicableRequirements,&QCheckBox::toggled,this,&ApplicabilityWidget::refreshMatrix);
  connect(m_archivedConfigurations,&QCheckBox::toggled,this,&ApplicabilityWidget::refreshMatrix);
  connect(m_matrix,&QTableWidget::cellDoubleClicked,this,[this](int row,int){
    if(row>=0&&row<m_rows.size())emit openRequirementRequested(m_rows[row].requirementId);
  });
  connect(exportButton,&QPushButton::clicked,this,[this]{QString path=QFileDialog::getSaveFileName(this,"Exporter la matrice","matrice-applicabilite.xlsx","Classeur Excel (*.xlsx)");if(path.isEmpty())return;if(!path.endsWith(".xlsx",Qt::CaseInsensitive))path+=".xlsx";ApplicabilityFilter f;f.ptRootId=m_pt->currentData().toInt();f.documentId=m_document->currentData().toInt();f.statusId=m_status->currentData().toInt();f.typeId=m_type->currentData().toInt();f.includeObsoleteRequirements=m_obsolete->isChecked();f.includeArchivedConfigurations=m_archivedConfigurations->isChecked();f.includeNotApplicableRequirements=m_notApplicableRequirements->isChecked();const auto r=m_service.exportXlsx(path,f);if(!r.success)QMessageBox::warning(this,"Export XLSX",r.message);else QMessageBox::information(this,"Export XLSX",r.message);});
}
void ApplicabilityWidget::setConnectionName(const QString&name){m_connection=name;m_service.setConnectionName(name);refresh();}
void ApplicabilityWidget::refresh(){QSqlDatabase db=QSqlDatabase::database(m_connection,false);if(m_connection.isEmpty()||!db.isValid()||!db.isOpen()){m_configurations->setRowCount(0);m_matrix->setRowCount(0);m_pt->clear();m_document->clear();m_status->clear();m_type->clear();m_configurationRecords.clear();m_rows.clear();m_rate->setText("Aucun projet ouvert.");return;}fill(m_pt,db,"SELECT ID,NAME FROM PT WHERE ARCHIVED=0 ORDER BY POSITION,NAME","Toutes les branches");fill(m_document,db,"SELECT D.ID,COALESCE((SELECT R.REFERENCE FROM DOCUMENT_REFERENCE R WHERE R.DOC_ID=D.ID AND R.IS_PRIMARY=1),D.TITLE) FROM DOCUMENT D ORDER BY D.TITLE","Tous les documents");fill(m_status,db,"SELECT ID,STATUS FROM REQ_STATUS ORDER BY ID","Tous les statuts");fill(m_type,db,"SELECT ID,TYPE FROM REQ_TYPE ORDER BY TYPE","Tous les types");refreshConfigurations();refreshMatrix();}
void ApplicabilityWidget::refreshConfigurations(){QString error;m_configurationRecords=m_service.configurations(true,&error);m_configurations->setRowCount(m_configurationRecords.size());for(int r=0;r<m_configurationRecords.size();++r){const auto&c=m_configurationRecords[r];QStringList values{c.code,c.label,c.description,QString::number(c.position),c.active?"Active":"Archivée",QString::number(c.useCount),c.updatedAt};for(int col=0;col<values.size();++col)m_configurations->setItem(r,col,new QTableWidgetItem(values[col]));m_configurations->item(r,0)->setData(Qt::UserRole,c.id);if(!c.active)for(int col=0;col<values.size();++col)m_configurations->item(r,col)->setForeground(Qt::gray);}}
void ApplicabilityWidget::editConfiguration(int id){ConfigurationRecord c=id<0?ConfigurationRecord():m_service.configuration(id);QDialog d(this);d.setWindowTitle(id<0?"Nouvelle configuration":"Modifier la configuration");auto form=new QFormLayout(&d);QLineEdit code(c.code),label(c.label);QTextEdit description;description.setPlainText(c.description);QSpinBox position;position.setRange(0,999999);position.setValue(c.position);QCheckBox active("Active");active.setChecked(c.active);form->addRow("Code unique",&code);form->addRow("Libellé",&label);form->addRow("Description",&description);form->addRow("Ordre",&position);form->addRow("État",&active);QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form->addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);if(d.exec()!=QDialog::Accepted)return;c.code=code.text();c.label=label.text();c.description=description.toPlainText();c.position=position.value();c.active=active.isChecked();auto result=m_service.saveConfiguration(c);if(!result.success)QMessageBox::warning(this,"Configuration",result.message);else{refresh();emit dataChanged();}}
void ApplicabilityWidget::refreshMatrix(){
  if(m_connection.isEmpty())return;
  ApplicabilityFilter f;
  f.ptRootId=m_pt->currentData().toInt();f.documentId=m_document->currentData().toInt();
  f.statusId=m_status->currentData().toInt();f.typeId=m_type->currentData().toInt();
  f.includeObsoleteRequirements=m_obsolete->isChecked();
  f.includeArchivedConfigurations=m_archivedConfigurations->isChecked();
  f.includeNotApplicableRequirements=m_notApplicableRequirements->isChecked();
  QString error;m_rows=m_service.matrix(f,&error);
  m_configurationRecords=m_service.configurations(f.includeArchivedConfigurations);
  constexpr int BaseColumns=7;
  m_matrix->setUpdatesEnabled(false);m_matrix->setSortingEnabled(false);
  m_matrix->clearContents();
  m_matrix->setColumnCount(BaseColumns+m_configurationRecords.size());
  QStringList headers{"Code","Titre","Statut","Type","Product Trees","Documents","État"};
  for(const auto&c:m_configurationRecords)headers<<c.code+(c.active?QString():" [ARCH]");
  m_matrix->setHorizontalHeaderLabels(headers);
  m_matrix->setColumnWidth(0,110);m_matrix->setColumnWidth(1,220);
  m_matrix->setColumnWidth(2,90);m_matrix->setColumnWidth(3,120);
  m_matrix->setColumnWidth(4,160);m_matrix->setColumnWidth(5,180);
  m_matrix->setColumnWidth(6,90);
  for(int c=0;c<m_configurationRecords.size();++c)m_matrix->setColumnWidth(BaseColumns+c,90);
  for(int c=0;c<m_matrix->columnCount();++c){auto*header=m_matrix->horizontalHeaderItem(c);if(header){header->setBackground(QBrush());header->setForeground(QBrush());}}
  m_matrix->setRowCount(m_rows.size());int applicableRequirements=0;
  for(int r=0;r<m_rows.size();++r){
    const auto&row=m_rows[r];if(!row.notApplicable)++applicableRequirements;
    const QString state=row.obsolete?(row.notApplicable?"OBS · N/A":"OBS"):(row.notApplicable?"N/A":"—");
    QStringList states;if(row.obsolete)states<<"OBS — Exigence obsolète";if(row.notApplicable)states<<"N/A — Non applicable au projet";
    const QString stateTip=(states.isEmpty()?QString("État normal"):states.join("\n"))+"\nDouble-cliquer pour ouvrir l'exigence.";
    QStringList base{row.code,row.title,row.status,row.type,row.productTrees,row.document,state};
    for(int c=0;c<base.size();++c){auto*item=new QTableWidgetItem(base[c]);if(c==6)item->setTextAlignment(Qt::AlignCenter);item->setToolTip(stateTip);if(row.obsolete){item->setBackground(QColor("#F3F0EC"));item->setForeground(QColor("#6B625A"));}else if(row.notApplicable){item->setBackground(QColor("#EAF2F8"));item->setForeground(QColor("#40566B"));}m_matrix->setItem(r,c,item);}
    for(int c=0;c<row.cells.size();++c){const auto&cell=row.cells[c];const bool archived=!m_configurationRecords[c].active;auto item=new QTableWidgetItem(row.notApplicable?"N/A":cell.applicable?"✓":"—");item->setTextAlignment(Qt::AlignCenter);QStringList tips=states;if(archived)tips<<"ARCH — Configuration archivée";if(!row.notApplicable)tips<<(cell.explicitValue?(cell.sourcePtId<0?"Valeur simple":"Exception PT")+(cell.comment.isEmpty()?QString():" — "+cell.comment):"Non renseigné");item->setToolTip(tips.join("\n")+"\nDouble-cliquer pour ouvrir l'exigence.");if(row.obsolete){item->setBackground(QColor("#F3F0EC"));item->setForeground(QColor("#6B625A"));}else if(row.notApplicable){item->setBackground(QColor("#EAF2F8"));item->setForeground(QColor("#40566B"));}else if(archived){item->setBackground(QColor("#EEF1F4"));item->setForeground(QColor("#667085"));}else if(cell.sourcePtId>=0)item->setBackground(QColor("#FFF2CC"));m_matrix->setItem(r,c+BaseColumns,item);}
  }
  for(int c=0;c<m_configurationRecords.size();++c)if(!m_configurationRecords[c].active){auto*header=m_matrix->horizontalHeaderItem(c+BaseColumns);if(header){header->setBackground(QColor("#D9DEE5"));header->setForeground(QColor("#667085"));header->setToolTip("ARCH — Configuration archivée");}}
  m_matrix->setUpdatesEnabled(true);m_matrix->viewport()->update();
  m_rate->setText(QString("<a href=\"details\">Taux d'applicabilité : %1 %</a> — %2 exigences applicables affichées sur %3 × %4 configurations. Double-cliquer une ligne pour ouvrir l'exigence.").arg(m_service.rate(m_rows),0,'f',2).arg(applicableRequirements).arg(m_rows.size()).arg(m_configurationRecords.size()));
}
