#include "documentcreationdialog.h"

#include "producttreeservice.h"

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
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

DocumentCreationDialog::DocumentCreationDialog(const QString &connectionName,
                                               QWidget *parent)
    : QDialog(parent), m_reference(new QLineEdit), m_secondary(new QLineEdit),
      m_title(new QLineEdit), m_template(new QLineEdit),
      m_description(new QTextEdit), m_type(new QComboBox),
      m_pt(new QComboBox), m_metadata(new QTableWidget(0, 2)),
      m_attachPt(new QCheckBox("Rattacher les exigences du PT du document")),
      m_includeChildren(new QCheckBox(
          "Inclure récursivement les exigences des éléments enfants")) {
  setWindowTitle("Créer un document");
  resize(720, 650);
  m_attachPt->setObjectName("attachPtRequirements");
  m_includeChildren->setObjectName("includeChildPtRequirements");
  m_attachPt->setChecked(true);
  m_metadata->setHorizontalHeaderLabels({"Clé", "Valeur"});
  m_metadata->horizontalHeader()->setStretchLastSection(true);

  QSqlDatabase db = QSqlDatabase::database(connectionName, false);
  QSqlQuery types("SELECT ID,TYPE FROM DOC_TYPE ORDER BY ID", db);
  while (types.next())
    m_type->addItem(types.value(1).toString(), types.value(0));
  ProductTreeService tree(connectionName);
  QSqlQuery pts("SELECT ID FROM PT WHERE ARCHIVED=0 ORDER BY POSITION,SEGMENT",
                db);
  while (pts.next()) {
    const int id = pts.value(0).toInt();
    m_pt->addItem(tree.fullCode(id), id);
  }

  auto *browse = new QPushButton("Parcourir…");
  auto *templateRow = new QHBoxLayout;
  templateRow->addWidget(m_template);
  templateRow->addWidget(browse);
  auto *addMetadata = new QPushButton("+ Métadonnée");
  auto *removeMetadata = new QPushButton("− Métadonnée");
  auto *metadataActions = new QHBoxLayout;
  metadataActions->addWidget(addMetadata);
  metadataActions->addWidget(removeMetadata);
  metadataActions->addStretch();
  auto *metadataBox = new QVBoxLayout;
  metadataBox->addWidget(m_metadata);
  metadataBox->addLayout(metadataActions);

  auto *form = new QFormLayout;
  form->addRow("Référence principale", m_reference);
  form->addRow("Références secondaires (;)", m_secondary);
  form->addRow("Titre", m_title);
  form->addRow("Type", m_type);
  form->addRow("Product Tree", m_pt);
  form->addRow("Description", m_description);
  form->addRow("Template DOCX", templateRow);
  form->addRow("Métadonnées", metadataBox);

  auto *scope = new QVBoxLayout;
  scope->addWidget(new QLabel("Contenu initial"));
  scope->addWidget(m_attachPt);
  scope->addWidget(m_includeChildren);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save |
                                       QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Save)->setText("Créer");
  auto *layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addLayout(scope);
  layout->addWidget(buttons);

  connect(m_includeChildren, &QCheckBox::toggled, this, [this](bool checked) {
    if (checked)
      m_attachPt->setChecked(true);
    m_attachPt->setEnabled(!checked);
  });
  connect(browse, &QPushButton::clicked, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, "Choisir un template DOCX", m_template->text(),
        "Documents Word (*.docx)");
    if (!path.isEmpty())
      m_template->setText(path);
  });
  connect(addMetadata, &QPushButton::clicked, this,
          [this] { m_metadata->insertRow(m_metadata->rowCount()); });
  connect(removeMetadata, &QPushButton::clicked, this, [this] {
    if (m_metadata->currentRow() >= 0)
      m_metadata->removeRow(m_metadata->currentRow());
  });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    if (m_reference->text().trimmed().isEmpty() ||
        m_title->text().trimmed().isEmpty()) {
      QMessageBox::warning(this, "Document",
                           "La référence et le titre sont obligatoires.");
      return;
    }
    if (m_type->currentIndex() < 0 || m_pt->currentIndex() < 0) {
      QMessageBox::warning(this, "Document",
                           "Le type et le Product Tree sont obligatoires.");
      return;
    }
    accept();
  });
}

DocumentRecord DocumentCreationDialog::record() const {
  DocumentRecord result;
  result.reference = m_reference->text();
  result.secondaryReferences =
      m_secondary->text().split(';', Qt::SkipEmptyParts);
  result.title = m_title->text();
  result.description = m_description->toPlainText();
  result.templatePath = m_template->text().trimmed();
  result.typeId = m_type->currentData().toInt();
  result.ptId = m_pt->currentData().toInt();
  for (int row = 0; row < m_metadata->rowCount(); ++row) {
    const QString key = m_metadata->item(row, 0)
                            ? m_metadata->item(row, 0)->text().trimmed()
                            : QString();
    const QString value = m_metadata->item(row, 1)
                              ? m_metadata->item(row, 1)->text()
                              : QString();
    if (!key.isEmpty())
      result.metadata[key] = value;
  }
  return result;
}

DocumentCreationOptions DocumentCreationDialog::options() const {
  return {m_attachPt->isChecked(), m_includeChildren->isChecked()};
}
