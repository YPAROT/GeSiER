#include "coveragedashboard.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtWidgets>
CoverageDashboard::CoverageDashboard(QWidget *p) : QWidget(p) {
  m_status = new QComboBox;
  m_obsolete = new QCheckBox("Inclure les exigences obsolètes");
  auto refreshButton = new QPushButton("Actualiser");
  auto top = new QHBoxLayout;
  top->addWidget(new QLabel("Périmètre :"));
  top->addWidget(m_status);
  top->addWidget(m_obsolete);
  top->addStretch();
  top->addWidget(refreshButton);
  m_state = new QLabel;
  m_grid = new QGridLayout;
  auto cards = new QWidget;
  cards->setLayout(m_grid);
  auto scroll = new QScrollArea;
  scroll->setWidgetResizable(true);
  scroll->setWidget(cards);
  auto root = new QVBoxLayout(this);
  root->addLayout(top);
  root->addWidget(m_state);
  root->addWidget(scroll);
  connect(refreshButton, &QPushButton::clicked, this,
          &CoverageDashboard::refresh);
  connect(m_status, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &CoverageDashboard::refresh);
  connect(m_obsolete, &QCheckBox::toggled, this, &CoverageDashboard::refresh);
}
void CoverageDashboard::setConnectionName(const QString &n) {
  m_connection = n;
  filters();
  refresh();
}
int CoverageDashboard::scalar(const QString &s) const {
  const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
  if (!db.isValid() || !db.isOpen())
    return 0;
  QSqlQuery q(s, db);
  return q.next() ? q.value(0).toInt() : 0;
}
QString CoverageDashboard::scope() const {
  QStringList w;
  if (!m_obsolete->isChecked())
    w << "NOT EXISTS(SELECT 1 FROM REQ_STATUS S WHERE S.ID=R.STATUS AND "
         "(UPPER(S.STATUS)='OBSOLETE' OR UPPER(S.SHORTCUT)='O'))";
  if (m_status->currentData().toInt() >= 0)
    w << QString("R.STATUS=%1").arg(m_status->currentData().toInt());
  return w.isEmpty() ? "1=1" : w.join(" AND ");
}
void CoverageDashboard::filters() {
  m_status->blockSignals(true);
  m_status->clear();
  m_status->addItem("Tous les statuts", -1);
  const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
  if (!db.isValid() || !db.isOpen()) {
    m_status->blockSignals(false);
    return;
  }
  QSqlQuery q("SELECT ID,STATUS FROM REQ_STATUS ORDER BY ID", db);
  while (q.next())
    m_status->addItem(q.value(1).toString(), q.value(0));
  m_status->blockSignals(false);
}
void CoverageDashboard::clearCards() {
  while (auto item = m_grid->takeAt(0)) {
    delete item->widget();
    delete item;
  }
}
void CoverageDashboard::refresh() {
  clearCards();
  QSqlDatabase db = QSqlDatabase::database(m_connection, false);
  if (!db.isValid() || !db.isOpen()) {
    m_state->setText("Aucun projet ouvert.");
    return;
  }
  QString s = scope();
  int req = scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + s),
      traceable = scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + s +
                         " AND COALESCE(IS_TRACE_ROOT,0)=0"),
      interfaces = scalar("SELECT COUNT(*) FROM INTERFACE");
  m_metrics = {
      {"Exigences", req, req, 1, "all"},
      {"Allocation Product Tree",
       scalar(
           "SELECT COUNT(*) FROM REQUIREMENT R WHERE " + s +
           " AND EXISTS(SELECT 1 FROM REQUIREMENT_PT X WHERE X.REQ_ID=R.ID)"),
       req, 1, "unallocated"},
      {"Traçabilité amont",
       scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + s +
              " AND COALESCE(IS_TRACE_ROOT,0)=0 AND EXISTS(SELECT 1 FROM "
              "REQUIREMENT_RELATION X WHERE X.TARGET_REQ_ID=R.ID AND X.TYPE_ID "
              "IN(1,2))"),
       traceable, 1, "untraced"},
      {"Couverture documentaire",
       scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + s +
              " AND EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE N.REQ_ID=R.ID)"),
       req, 1, "undocumented"},
      {"Planification vérification",
       scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + s +
              " AND EXISTS(SELECT 1 FROM REQUIREMENT_VERIFICATION V "
              "WHERE V.REQ_ID=R.ID AND V.METHOD_ID IS NOT NULL AND "
              "V.VERIFICATION_LEVEL_PT_ID IS NOT NULL)"),
       req, 1, "unverified"},
      {"Applicabilité définie",
       scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + s +
              " AND EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A WHERE "
              "A.REQ_ID=R.ID)"),
       req, 1, "no-applicability"},
      {"Interfaces couvertes ICD",
       scalar("SELECT COUNT(*) FROM INTERFACE I WHERE EXISTS(SELECT 1 FROM "
              "INTERFACE_DOCUMENT X WHERE X.INTERFACE_ID=I.ID)"),
       interfaces, 4, "uncovered"},
      {"Documents", scalar("SELECT COUNT(*) FROM DOCUMENT"),
       scalar("SELECT COUNT(*) FROM DOCUMENT"), 2, "all"},
      {"Publications récentes",
       scalar("SELECT COUNT(*) FROM DOCUMENT_EXPORT WHERE "
              "EXPORT_KIND='PUBLICATION' AND EXPORTED_AT>=DATETIME('now','-30 "
              "day')"),
       scalar("SELECT COUNT(*) FROM DOCUMENT_EXPORT WHERE "
              "EXPORT_KIND='PUBLICATION'"),
       2, "publications"},
      {"Changements décidés et finalisés",
       scalar("SELECT COUNT(*) FROM CHANGE_ITEM C JOIN CHANGE_STATUS S ON "
              "S.ID=C.STATUS_ID WHERE C.ARCHIVED=0 AND S.IS_FINAL=1 AND "
              "TRIM(COALESCE(C.DECISION,''))<>''"),
       scalar("SELECT COUNT(*) FROM CHANGE_ITEM WHERE ARCHIVED=0"), 6,
       "incomplete"}};
  m_state->setText(
      req == 0 ? "Le projet ne contient encore aucune exigence."
               : QString("%1 exigence(s) dans le périmètre courant.").arg(req));
  for (int i = 0; i < m_metrics.size(); ++i) {
    auto &m = m_metrics[i];
    int pct = m.total ? qRound(100.0 * m.covered / m.total) : 0;
    auto button = new QToolButton;
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setText(QString("%1\n\n%2 %\n%3 / %4")
                        .arg(m.label)
                        .arg(pct)
                        .arg(m.covered)
                        .arg(m.total));
    button->setMinimumSize(210, 125);
    button->setStyleSheet(QString("QToolButton{font-size:14px;text-align:left;"
                                  "padding:16px;border:1px solid "
                                  "#bbb;border-radius:8px;background:%1}"
                                  "QToolButton:hover{border:2px solid #4080c0}")
                              .arg(pct >= 90   ? "#e3f3e8"
                                   : pct >= 60 ? "#fff4d6"
                                               : "#fbe4e2"));
    connect(button, &QToolButton::clicked, this,
            [this, m] { emit navigateRequested(m.page, m.filter); });
    m_grid->addWidget(button, i / 3, i % 3);
  }
}
