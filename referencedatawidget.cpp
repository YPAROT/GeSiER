#include "referencedatawidget.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QPushButton *button(const QString &text, const QString &name, QWidget *parent) {
  auto *result = new QPushButton(text, parent); result->setObjectName(name); return result;
}
void configure(QTableWidget *table, const QStringList &headers) {
  table->setColumnCount(headers.size()); table->setHorizontalHeaderLabels(headers);
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->verticalHeader()->hide(); table->horizontalHeader()->setStretchLastSection(true);
}
}

ReferenceDataWidget::ReferenceDataWidget(QWidget *parent) : QWidget(parent) {
  auto *tabs = new QTabWidget(this);
  auto *methodsPage = new QWidget(tabs); auto *methodsLayout = new QVBoxLayout(methodsPage);
  m_methods = new QTableWidget(methodsPage); m_methods->setObjectName("verificationMethodsTable"); configure(m_methods,{"Méthode"});
  m_methodLabel = new QLineEdit(methodsPage); m_methodLabel->setObjectName("verificationMethodLabel");
  m_methodLabel->setPlaceholderText("Libellé de la méthode");
  auto *methodForm = new QFormLayout; methodForm->addRow("Libellé",m_methodLabel);
  auto *methodActions = new QHBoxLayout; auto *addMethod=button("Ajouter","addVerificationMethod",methodsPage); auto *editMethod=button("Modifier","editVerificationMethod",methodsPage); auto *deleteMethod=button("Supprimer","deleteVerificationMethod",methodsPage);
  methodActions->addWidget(addMethod);methodActions->addWidget(editMethod);methodActions->addWidget(deleteMethod);methodActions->addStretch();
  methodsLayout->addWidget(m_methods);methodsLayout->addLayout(methodForm);methodsLayout->addLayout(methodActions);

  auto *typesPage = new QWidget(tabs); auto *typesLayout = new QVBoxLayout(typesPage);
  m_types = new QTableWidget(typesPage); m_types->setObjectName("requirementTypesTable"); configure(m_types,{"Code","Libellé"});
  m_typeCode = new QLineEdit(typesPage); m_typeCode->setObjectName("requirementTypeCode"); m_typeCode->setPlaceholderText("Facultatif");
  m_typeLabel = new QLineEdit(typesPage); m_typeLabel->setObjectName("requirementTypeLabel");
  auto *typeForm = new QFormLayout;typeForm->addRow("Code court",m_typeCode);typeForm->addRow("Libellé",m_typeLabel);
  auto *typeActions = new QHBoxLayout;auto *addType=button("Ajouter","addRequirementType",typesPage);auto *editType=button("Modifier","editRequirementType",typesPage);auto *deleteType=button("Supprimer","deleteRequirementType",typesPage);
  typeActions->addWidget(addType);typeActions->addWidget(editType);typeActions->addWidget(deleteType);typeActions->addStretch();
  typesLayout->addWidget(m_types);typesLayout->addLayout(typeForm);typesLayout->addLayout(typeActions);
  auto *formatPage = new QWidget(tabs); auto *formatLayout = new QVBoxLayout(formatPage);
  auto *formatForm = new QFormLayout;
  m_codeSeparator = new QLineEdit(formatPage); m_codeSeparator->setObjectName("requirementCodeSeparator"); m_codeSeparator->setMaxLength(1);
  m_codeMarker = new QLineEdit(formatPage); m_codeMarker->setObjectName("requirementCodeMarker"); m_codeMarker->setMaxLength(5);
  m_codeDigits = new QSpinBox(formatPage); m_codeDigits->setObjectName("requirementCodeDigits"); m_codeDigits->setRange(1,9);
  m_codeIncludeRoot = new QCheckBox("Inclure la racine du Product Tree",formatPage); m_codeIncludeRoot->setObjectName("requirementCodeIncludeRoot");
  m_codePreview = new QLabel(formatPage); m_codePreview->setObjectName("requirementCodePreview");
  auto *saveFormat = button("Enregistrer le format","saveRequirementCodeFormat",formatPage);
  formatForm->addRow("Séparateur",m_codeSeparator); formatForm->addRow("Marqueur d'exigence",m_codeMarker);
  formatForm->addRow("Nombre de chiffres",m_codeDigits); formatForm->addRow(QString(),m_codeIncludeRoot);
  formatForm->addRow("Aperçu",m_codePreview);
  formatLayout->addLayout(formatForm); formatLayout->addWidget(saveFormat,0,Qt::AlignLeft); formatLayout->addStretch();
  tabs->addTab(formatPage,"Format des codes"); tabs->addTab(methodsPage,"Méthodes de vérification"); tabs->addTab(typesPage,"Types d'exigence");
  auto *root = new QVBoxLayout(this);root->addWidget(tabs);

  connect(m_methods,&QTableWidget::itemSelectionChanged,this,[this]{int row=m_methods->currentRow();if(row>=0)m_methodLabel->setText(m_methods->item(row,0)->text());});
  connect(m_types,&QTableWidget::itemSelectionChanged,this,[this]{int row=m_types->currentRow();if(row>=0){m_typeCode->setText(m_types->item(row,0)->text());m_typeLabel->setText(m_types->item(row,1)->text());}});
  connect(addMethod,&QPushButton::clicked,this,[this]{showResult(m_service.addVerificationMethod(m_methodLabel->text()));});
  connect(editMethod,&QPushButton::clicked,this,[this]{int id=selectedId(m_methods);if(id<0){QMessageBox::information(this,"Méthodes de vérification","Sélectionnez une méthode à modifier.");return;}showResult(m_service.updateVerificationMethod(id,m_methodLabel->text()));});
  connect(deleteMethod,&QPushButton::clicked,this,[this]{int id=selectedId(m_methods);if(id<0){QMessageBox::information(this,"Méthodes de vérification","Sélectionnez une méthode à supprimer.");return;}if(QMessageBox::question(this,"Supprimer la méthode","Supprimer la méthode sélectionnée ?") == QMessageBox::Yes)showResult(m_service.deleteVerificationMethod(id));});
  connect(addType,&QPushButton::clicked,this,[this]{showResult(m_service.addRequirementType(m_typeCode->text(),m_typeLabel->text()));});
  connect(editType,&QPushButton::clicked,this,[this]{int id=selectedId(m_types);if(id<0){QMessageBox::information(this,"Types d'exigence","Sélectionnez un type à modifier.");return;}showResult(m_service.updateRequirementType(id,m_typeCode->text(),m_typeLabel->text()));});
  connect(deleteType,&QPushButton::clicked,this,[this]{int id=selectedId(m_types);if(id<0){QMessageBox::information(this,"Types d'exigence","Sélectionnez un type à supprimer.");return;}if(QMessageBox::question(this,"Supprimer le type","Supprimer le type sélectionné ?") == QMessageBox::Yes)showResult(m_service.deleteRequirementType(id));});
  connect(m_codeSeparator,&QLineEdit::textChanged,this,&ReferenceDataWidget::updateCodePreview);
  connect(m_codeMarker,&QLineEdit::textChanged,this,&ReferenceDataWidget::updateCodePreview);
  connect(m_codeDigits,QOverload<int>::of(&QSpinBox::valueChanged),this,&ReferenceDataWidget::updateCodePreview);
  connect(m_codeIncludeRoot,&QCheckBox::toggled,this,&ReferenceDataWidget::updateCodePreview);
  connect(saveFormat,&QPushButton::clicked,this,[this]{RequirementCodeFormat format;format.separator=m_codeSeparator->text().isEmpty()?QChar():m_codeSeparator->text().front();format.marker=m_codeMarker->text();format.digits=m_codeDigits->value();format.includeRoot=m_codeIncludeRoot->isChecked();showResult(m_service.saveCodeFormat(format));});
}

void ReferenceDataWidget::setConnectionName(const QString &name){m_connection=name;m_service.setConnectionName(name);refresh();}
void ReferenceDataWidget::releaseDatabase(){setConnectionName({});}
int ReferenceDataWidget::selectedId(QTableWidget *table) const {int row=table->currentRow();return row<0?-1:table->item(row,0)->data(Qt::UserRole).toInt();}
void ReferenceDataWidget::showResult(const ReferenceDataResult &result){if(!result.success){QMessageBox::warning(this,"Paramètres",result.message);return;}m_methodLabel->clear();m_typeCode->clear();m_typeLabel->clear();refresh();emit dataChanged();}
void ReferenceDataWidget::refresh(){refreshCodeFormat();refreshMethods();refreshTypes();}
void ReferenceDataWidget::refreshMethods(){QString error;auto rows=m_service.verificationMethods(&error);m_methods->setRowCount(rows.size());for(int r=0;r<rows.size();++r){auto*item=new QTableWidgetItem(rows[r].label);item->setData(Qt::UserRole,rows[r].id);m_methods->setItem(r,0,item);}if(!error.isEmpty()&&!m_connection.isEmpty())QMessageBox::warning(this,"Paramètres",error);}
void ReferenceDataWidget::refreshTypes(){QString error;auto rows=m_service.requirementTypes(&error);m_types->setRowCount(rows.size());for(int r=0;r<rows.size();++r){auto*code=new QTableWidgetItem(rows[r].code);code->setData(Qt::UserRole,rows[r].id);m_types->setItem(r,0,code);m_types->setItem(r,1,new QTableWidgetItem(rows[r].label));}if(!error.isEmpty()&&!m_connection.isEmpty())QMessageBox::warning(this,"Paramètres",error);}
void ReferenceDataWidget::refreshCodeFormat(){const auto format=m_service.codeFormat();m_codeSeparator->setText(QString(format.separator));m_codeMarker->setText(format.marker);m_codeDigits->setValue(format.digits);m_codeIncludeRoot->setChecked(format.includeRoot);updateCodePreview();}
void ReferenceDataWidget::updateCodePreview(){const QString separator=m_codeSeparator->text();const QString path=m_codeIncludeRoot->isChecked()?"RACINE"+separator+"PTLVL1":"PTLVL1";m_codePreview->setText(path+separator+m_codeMarker->text()+separator+QString("%1").arg(0,m_codeDigits->value(),10,QChar('0')));}
