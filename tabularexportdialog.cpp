#include "tabularexportdialog.h"

#include "tabularservice.h"

#include <QtWidgets>

TabularExportDialog::TabularExportDialog(const QString &scope,
                                         const QStringList &headers,
                                         QWidget *parent)
    : QDialog(parent), m_scope(scope) {
  setWindowTitle("Configurer l'export tabulaire");
  resize(680, 520);
  m_sheetName = new QLineEdit("Données");
  m_columns = new QTableWidget(0, 2);
  m_columns->setHorizontalHeaderLabels({"Colonne", "En-tête exporté"});
  m_columns->horizontalHeader()->setStretchLastSection(true);
  const TabularProfile profile = TabularService::loadProfile(scope);
  if (!profile.sheetName.isEmpty()) m_sheetName->setText(profile.sheetName);
  QSet<int> inserted;
  auto addRow = [&](int column, const QString &outputHeader) {
    if (column < 0 || column >= headers.size() || inserted.contains(column)) return;
    inserted << column;
    const int row = m_columns->rowCount();
    m_columns->insertRow(row);
    auto *source = new QTableWidgetItem(headers[column]);
    source->setData(Qt::UserRole, column);
    source->setFlags(source->flags() | Qt::ItemIsUserCheckable);
    source->setCheckState(Qt::Checked);
    m_columns->setItem(row, 0, source);
    m_columns->setItem(row, 1, new QTableWidgetItem(
                                  outputHeader.isEmpty() ? headers[column]
                                                         : outputHeader));
  };
  for (const auto &mapping : profile.mappings) addRow(mapping.sourceColumn, mapping.outputHeader);
  for (int column = 0; column < headers.size(); ++column) addRow(column, headers[column]);
  auto *up = new QPushButton("Monter");
  auto *down = new QPushButton("Descendre");
  auto *actions = new QHBoxLayout;
  actions->addWidget(up); actions->addWidget(down); actions->addStretch();
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText("Exporter");
  auto *layout = new QVBoxLayout(this);
  auto *form = new QFormLayout; form->addRow("Nom de la feuille", m_sheetName);
  layout->addLayout(form); layout->addWidget(m_columns); layout->addLayout(actions); layout->addWidget(buttons);
  connect(up, &QPushButton::clicked, this, [this] { moveCurrent(-1); });
  connect(down, &QPushButton::clicked, this, [this] { moveCurrent(1); });
  connect(buttons, &QDialogButtonBox::accepted, this, &TabularExportDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TabularExportDialog::moveCurrent(int offset) {
  const int row = m_columns->currentRow(), target = row + offset;
  if (row < 0 || target < 0 || target >= m_columns->rowCount()) return;
  for (int column = 0; column < m_columns->columnCount(); ++column) {
    QTableWidgetItem *first = m_columns->takeItem(row, column);
    QTableWidgetItem *second = m_columns->takeItem(target, column);
    m_columns->setItem(row, column, second);
    m_columns->setItem(target, column, first);
  }
  m_columns->setCurrentCell(target, 0);
}

QList<int> TabularExportDialog::columns() const {
  QList<int> result;
  for (int row = 0; row < m_columns->rowCount(); ++row)
    if (m_columns->item(row, 0)->checkState() == Qt::Checked)
      result << m_columns->item(row, 0)->data(Qt::UserRole).toInt();
  return result;
}

QStringList TabularExportDialog::outputHeaders() const {
  QStringList result;
  for (int row = 0; row < m_columns->rowCount(); ++row)
    if (m_columns->item(row, 0)->checkState() == Qt::Checked)
      result << m_columns->item(row, 1)->text().trimmed();
  return result;
}

QString TabularExportDialog::sheetName() const {
  return m_sheetName->text().trimmed().isEmpty() ? "Données"
                                                 : m_sheetName->text().trimmed();
}

void TabularExportDialog::accept() {
  if (columns().isEmpty()) {
    QMessageBox::warning(this, "Export tabulaire", "Sélectionnez au moins une colonne.");
    return;
  }
  TabularProfile profile; profile.name = "default"; profile.sheetName = sheetName();
  const auto selected = columns(); const auto headers = outputHeaders();
  for (int index = 0; index < selected.size(); ++index)
    profile.mappings << TabularColumnMapping{QString::number(selected[index]), {},
                                             headers[index], selected[index],
                                             TabularTransform::None};
  TabularService::saveProfile(m_scope, profile);
  QDialog::accept();
}
