#include "coveragedashboard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtMath>

CoverageDashboard::CoverageDashboard(QWidget *parent) : QWidget(parent)
{
    m_statusFilter = new QComboBox(this);
    m_statusFilter->addItem("Tous les statuts", QVariant());
    m_includeObsolete = new QCheckBox("Inclure les exigences obsolètes", this);
    m_refreshButton = new QPushButton("Actualiser", this);
    m_scopeLabel = new QLabel(this);
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"Indicateur", "Couverture", "Résultat", "Éléments incomplets"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    QHBoxLayout *filters = new QHBoxLayout;
    filters->addWidget(new QLabel("Périmètre :", this));
    filters->addWidget(m_statusFilter);
    filters->addWidget(m_includeObsolete);
    filters->addStretch();
    filters->addWidget(m_refreshButton);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(filters);
    layout->addWidget(m_scopeLabel);
    layout->addWidget(m_table);

    connect(m_refreshButton, &QPushButton::clicked, this, &CoverageDashboard::refresh);
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CoverageDashboard::refresh);
    connect(m_includeObsolete, &QCheckBox::toggled, this, &CoverageDashboard::refresh);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &CoverageDashboard::showMissing);
}

void CoverageDashboard::setConnectionName(const QString &connectionName)
{
    m_connectionName = connectionName;
    refreshFilters();
    refresh();
}

QString CoverageDashboard::requirementScope(const QString &alias) const
{
    QStringList clauses;
    if (!m_includeObsolete->isChecked())
        clauses << QString("NOT EXISTS (SELECT 1 FROM REQ_STATUS S WHERE S.ID=%1.STATUS AND (UPPER(S.STATUS)='OBSOLETE' OR UPPER(S.SHORTCUT)='O'))").arg(alias);
    if (m_statusFilter->currentData().isValid())
        clauses << QString("%1.STATUS=%2").arg(alias).arg(m_statusFilter->currentData().toInt());
    return clauses.isEmpty() ? "1=1" : clauses.join(" AND ");
}

int CoverageDashboard::scalar(const QString &queryText, bool *ok) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    const bool success = query.exec(queryText) && query.next();
    if (ok) *ok = success;
    return success ? query.value(0).toInt() : 0;
}

void CoverageDashboard::refreshFilters()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) return;
    const QVariant old = m_statusFilter->currentData();
    m_statusFilter->blockSignals(true);
    m_statusFilter->clear(); m_statusFilter->addItem("Tous les statuts", QVariant());
    QSqlQuery query("SELECT ID,STATUS FROM REQ_STATUS ORDER BY ID", db);
    while (query.next()) m_statusFilter->addItem(query.value(1).toString(), query.value(0));
    const int index = m_statusFilter->findData(old);
    if (index >= 0) m_statusFilter->setCurrentIndex(index);
    m_statusFilter->blockSignals(false);
}

void CoverageDashboard::refresh()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) { m_table->setRowCount(0); m_scopeLabel->setText("Aucun projet ouvert."); return; }
    const QString scope = requirementScope();
    const int requirements = scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + scope);
    const int traceable = scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + scope + " AND COALESCE(R.IS_TRACE_ROOT,0)=0");
    const int interfaces = scalar("SELECT COUNT(*) FROM INTERFACE");
    const int changes = scalar("SELECT COUNT(*) FROM CHANGE_ITEM");

    m_metrics = {
        {"Allocation product tree", scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + scope + " AND EXISTS(SELECT 1 FROM REQUIREMENT_PT X WHERE X.REQ_ID=R.ID)"), requirements,
         "SELECT R.CODE,R.TITLE FROM REQUIREMENT R WHERE " + scope + " AND NOT EXISTS(SELECT 1 FROM REQUIREMENT_PT X WHERE X.REQ_ID=R.ID)"},
        {"Traçabilité amont", scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + scope + " AND COALESCE(R.IS_TRACE_ROOT,0)=0 AND EXISTS(SELECT 1 FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID WHERE X.TARGET_REQ_ID=R.ID AND T.CODE IN ('DECOMPOSE','DERIVES_FROM'))"), traceable,
         "SELECT R.CODE,R.TITLE FROM REQUIREMENT R WHERE " + scope + " AND COALESCE(R.IS_TRACE_ROOT,0)=0 AND NOT EXISTS(SELECT 1 FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID WHERE X.TARGET_REQ_ID=R.ID AND T.CODE IN ('DECOMPOSE','DERIVES_FROM'))"},
        {"Couverture documentaire", scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + scope + " AND EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE N.REQ_ID=R.ID AND N.NODE_TYPE='REQUIREMENT')"), requirements,
         "SELECT R.CODE,R.TITLE FROM REQUIREMENT R WHERE " + scope + " AND NOT EXISTS(SELECT 1 FROM DOCUMENT_NODE N WHERE N.REQ_ID=R.ID AND N.NODE_TYPE='REQUIREMENT')"},
        {"Planification de vérification", scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + scope + " AND R.VERIF_METHOD IS NOT NULL AND LENGTH(TRIM(COALESCE(R.VERIF_LEVEL,'')))>0 AND (LENGTH(TRIM(COALESCE(R.VERIF_PROCEDURE,'')))>0 OR LENGTH(TRIM(COALESCE(R.REDMINE_REF,'')))>0)"), requirements,
         "SELECT R.CODE,R.TITLE FROM REQUIREMENT R WHERE " + scope + " AND NOT (R.VERIF_METHOD IS NOT NULL AND LENGTH(TRIM(COALESCE(R.VERIF_LEVEL,'')))>0 AND (LENGTH(TRIM(COALESCE(R.VERIF_PROCEDURE,'')))>0 OR LENGTH(TRIM(COALESCE(R.REDMINE_REF,'')))>0))"},
        {"Applicabilité définie", scalar("SELECT COUNT(*) FROM REQUIREMENT R WHERE " + scope + " AND EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A WHERE A.REQ_ID=R.ID)"), requirements,
         "SELECT R.CODE,R.TITLE FROM REQUIREMENT R WHERE " + scope + " AND NOT EXISTS(SELECT 1 FROM REQUIREMENT_APPLICABILITY A WHERE A.REQ_ID=R.ID)"},
        {"Interfaces couvertes par un ICD", scalar("SELECT COUNT(*) FROM INTERFACE I WHERE EXISTS(SELECT 1 FROM INTERFACE_DOCUMENT X WHERE X.INTERFACE_ID=I.ID)"), interfaces,
         "SELECT COALESCE(I.CODE,'IF-'||I.ID),I.DESCRIPTION FROM INTERFACE I WHERE NOT EXISTS(SELECT 1 FROM INTERFACE_DOCUMENT X WHERE X.INTERFACE_ID=I.ID)"},
        {"Changements clôturés", scalar("SELECT COUNT(*) FROM CHANGE_ITEM C WHERE LENGTH(TRIM(COALESCE(C.DECISION,'')))>0 AND UPPER(C.STATUS) IN ('CLOSED','CLOSE','CLOTURE','CLOTURÉ','APPROVED','REJECTED')"), changes,
         "SELECT C.CODE,C.DESCRIPTION FROM CHANGE_ITEM C WHERE LENGTH(TRIM(COALESCE(C.DECISION,'')))=0 OR UPPER(C.STATUS) NOT IN ('CLOSED','CLOSE','CLOTURE','CLOTURÉ','APPROVED','REJECTED')"}
    };

    m_scopeLabel->setText(QString("%1 exigence(s) dans le périmètre — double-cliquer une ligne pour afficher les éléments incomplets.").arg(requirements));
    m_table->setRowCount(m_metrics.size());
    for (int row=0; row<m_metrics.size(); ++row) {
        const Metric &metric = m_metrics.at(row);
        const int percent = metric.total == 0 ? 0 : qRound(100.0 * metric.covered / metric.total);
        m_table->setItem(row,0,new QTableWidgetItem(metric.label));
        QProgressBar *bar = new QProgressBar(m_table); bar->setRange(0,100); bar->setValue(percent); bar->setFormat("%p %");
        if (percent < 60) bar->setStyleSheet("QProgressBar::chunk{background:#c94b45;}");
        else if (percent < 90) bar->setStyleSheet("QProgressBar::chunk{background:#d7a928;}");
        else bar->setStyleSheet("QProgressBar::chunk{background:#4b9b69;}");
        m_table->setCellWidget(row,1,bar);
        m_table->setItem(row,2,new QTableWidgetItem(QString("%1 / %2").arg(metric.covered).arg(metric.total)));
        m_table->setItem(row,3,new QTableWidgetItem(QString::number(qMax(0, metric.total-metric.covered))));
    }
}

void CoverageDashboard::showMissing(int row, int)
{
    if (row < 0 || row >= m_metrics.size()) return;
    QSqlQuery query(m_metrics.at(row).detailsQuery, QSqlDatabase::database(m_connectionName));
    QStringList lines;
    while (query.next() && lines.size() < 100)
        lines << QString("%1 — %2").arg(query.value(0).toString(), query.value(1).toString());
    if (lines.isEmpty()) lines << "Aucun élément incomplet dans ce périmètre.";
    QMessageBox::information(this, m_metrics.at(row).label, lines.join('\n'));
}
