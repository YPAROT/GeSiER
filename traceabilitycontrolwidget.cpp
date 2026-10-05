#include "traceabilitycontrolwidget.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

TraceabilityControlWidget::TraceabilityControlWidget(QWidget *parent)
    : QWidget(parent) {
  m_search = new QLineEdit;
  m_search->setPlaceholderText("Rechercher par code ou titre");
  m_status = new QComboBox;
  m_applicable = new QComboBox;
  m_takenIntoAccount = new QComboBox;
  m_rootConform = new QComboBox;
  m_includeObsolete = new QCheckBox("Inclure les exigences obsolètes");
  fillBooleanFilter(m_applicable, "Toute applicabilité");
  fillBooleanFilter(m_takenIntoAccount, "Toute prise en compte");
  fillBooleanFilter(m_rootConform, "Toute conformité PT root");

  auto *filters = new QHBoxLayout;
  filters->addWidget(m_search, 2);
  filters->addWidget(m_status);
  filters->addWidget(m_applicable);
  filters->addWidget(m_takenIntoAccount);
  filters->addWidget(m_rootConform);
  filters->addWidget(m_includeObsolete);

  m_state = new QLabel;
  m_state->setWordWrap(true);
  m_table = new QTableWidget(0, 10);
  m_table->setObjectName("traceabilityControlTable");
  m_table->setHorizontalHeaderLabels(
      {"Code", "Titre", "Statut", "Applicabilité au projet", "PT principal",
       "PT root conforme", "Relations qualifiantes", "Types de relations",
       "Prise en compte", "Diagnostic"});
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSortingEnabled(true);
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);

  auto *layout = new QVBoxLayout(this);
  layout->addLayout(filters);
  layout->addWidget(m_state);
  layout->addWidget(m_table, 1);

  connect(m_search, &QLineEdit::textChanged, this,
          &TraceabilityControlWidget::refresh);
  for (QComboBox *combo :
       {m_status, m_applicable, m_takenIntoAccount, m_rootConform})
    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TraceabilityControlWidget::refresh);
  connect(m_includeObsolete, &QCheckBox::toggled, this,
          &TraceabilityControlWidget::refresh);
  connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
    if (row >= 0 && m_table->item(row, 0))
      emit openRequirementRequested(
          m_table->item(row, 0)->data(Qt::UserRole).toInt());
  });
}

void TraceabilityControlWidget::fillBooleanFilter(QComboBox *combo,
                                                  const QString &allLabel) {
  combo->addItem(allLabel, -1);
  combo->addItem("Oui", 1);
  combo->addItem("Non", 0);
}

void TraceabilityControlWidget::setConnectionName(
    const QString &connectionName) {
  m_connectionName = connectionName;
  m_service.setConnectionName(connectionName);
  refreshStatuses();
  refresh();
}

void TraceabilityControlWidget::refreshStatuses() {
  const QVariant selected = m_status->currentData();
  m_status->blockSignals(true);
  m_status->clear();
  m_status->addItem("Tous les statuts", -1);
  const QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
  if (db.isValid() && db.isOpen()) {
    QSqlQuery query("SELECT ID,STATUS FROM REQ_STATUS ORDER BY ID", db);
    while (query.next())
      m_status->addItem(query.value(1).toString(), query.value(0));
  }
  const int index = m_status->findData(selected);
  m_status->setCurrentIndex(index >= 0 ? index : 0);
  m_status->blockSignals(false);
}

TraceabilityFilter TraceabilityControlWidget::filter() const {
  TraceabilityFilter result;
  result.text = m_search->text();
  result.statusId = m_status->currentData().toInt();
  result.applicable = m_applicable->currentData().toInt();
  result.takenIntoAccount = m_takenIntoAccount->currentData().toInt();
  result.rootConform = m_rootConform->currentData().toInt();
  result.includeObsolete = m_includeObsolete->isChecked();
  return result;
}

void TraceabilityControlWidget::refresh() {
  const QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
  if (m_connectionName.isEmpty() || !db.isValid() || !db.isOpen()) {
    m_table->setRowCount(0);
    m_state->setText("Aucun projet ouvert.");
    return;
  }
  QString error;
  const QList<TraceabilityControlRow> rows = m_service.rows(filter(), &error);
  if (!error.isEmpty()) {
    m_table->setRowCount(0);
    m_state->setText(error);
    return;
  }

  QStringList state;
  if (m_service.activeRootId(&error) < 0)
    state << "Aucune racine Product Tree active : la conformité au root ne peut pas être vérifiée.";
  if (m_service.activeConfigurationCount(&error) == 0)
    state << "Aucune configuration active : les racines sont considérées non applicables au projet.";
  state << QString("%1 exigence(s) haut niveau affichée(s).").arg(rows.size());
  m_state->setText(state.join(" "));

  m_table->setSortingEnabled(false);
  m_table->setRowCount(rows.size());
  for (int rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
    const TraceabilityControlRow &row = rows[rowIndex];
    const QStringList values{
        row.code,
        row.title,
        row.status,
        row.applicable ? "Applicable" : "Non applicable au projet",
        row.primaryPt.isEmpty() ? "—" : row.primaryPt,
        row.rootConform ? "Oui" : "Non",
        QString::number(row.qualifyingRelationCount),
        row.relationTypes.isEmpty() ? "—" : row.relationTypes.join(" | "),
        row.applicable ? (row.takenIntoAccount ? "Oui" : "Non")
                       : "Non requise",
        row.diagnostic};
    for (int column = 0; column < values.size(); ++column) {
      auto *item = new QTableWidgetItem(values[column]);
      item->setData(Qt::UserRole, row.requirementId);
      m_table->setItem(rowIndex, column, item);
    }
  }
  m_table->setSortingEnabled(true);
}
