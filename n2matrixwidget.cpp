#include "n2matrixwidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

N2MatrixWidget::N2MatrixWidget(QWidget *parent) : QWidget(parent) {
  m_onlyWithInterfaces =
      new QCheckBox("Masquer les éléments sans interface", this);
  m_refresh = new QPushButton("Actualiser", this);
  m_matrix = new QTableWidget(this);
  m_matrix->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_matrix->setSelectionMode(QAbstractItemView::SingleSelection);
  QLabel *legend =
      new QLabel("Légende : vert = couverture ICD complète — orange = "
                 "couverture partielle ou absente — blanc = aucune interface — "
                 "×N / •N = interfaces / types multiples",
                 this);
  legend->setWordWrap(true);
  legend->setAccessibleName("Légende de la matrice N deux");
  m_rate = new QLabel(this);
  QHBoxLayout *tools = new QHBoxLayout;
  tools->addWidget(legend);
  tools->addStretch();
  tools->addWidget(m_onlyWithInterfaces);
  tools->addWidget(m_refresh);
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addLayout(tools);
  layout->addWidget(m_rate);
  layout->addWidget(m_matrix);
  connect(m_refresh, &QPushButton::clicked, this, &N2MatrixWidget::refresh);
  connect(m_onlyWithInterfaces, &QCheckBox::toggled, this,
          &N2MatrixWidget::refresh);
  connect(m_matrix, &QTableWidget::cellDoubleClicked, this,
          &N2MatrixWidget::showDetails);
}

void N2MatrixWidget::setConnectionName(const QString &connectionName) {
  m_connectionName = connectionName;
  refresh();
}

void N2MatrixWidget::refresh() {
  QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
  m_matrix->clear();
  m_ptIds.clear();
  if (!db.isValid() || !db.isOpen()) {
    m_matrix->setRowCount(0);
    m_matrix->setColumnCount(0);
    return;
  }
  QString condition;
  if (m_onlyWithInterfaces->isChecked())
    condition =
        " WHERE EXISTS(SELECT 1 FROM INTERFACE I WHERE "
        "COALESCE(I.ARCHIVED,0)=0 AND (I.ELEMENT1=PT.ID OR I.ELEMENT2=PT.ID))";
  else
    condition = " WHERE COALESCE(PT.ARCHIVED,0)=0";
  QSqlQuery pt("SELECT ID,NAME FROM PT" + condition + " ORDER BY POSITION,ID",
               db);
  QStringList names;
  while (pt.next()) {
    m_ptIds << pt.value(0).toInt();
    names << pt.value(1).toString();
  }
  m_matrix->setRowCount(names.size());
  m_matrix->setColumnCount(names.size());
  m_matrix->setHorizontalHeaderLabels(names);
  m_matrix->setVerticalHeaderLabels(names);
  int all = 0, coveredAll = 0;
  for (int row = 0; row < m_ptIds.size(); ++row) {
    for (int col = 0; col < m_ptIds.size(); ++col) {
      QTableWidgetItem *item = new QTableWidgetItem;
      item->setTextAlignment(Qt::AlignCenter);
      m_matrix->setItem(row, col, item);
      if (row == col) {
        item->setText("—");
        item->setBackground(QColor("#dedede"));
        continue;
      }
      QSqlQuery query(db);
      query.prepare(
          "SELECT COUNT(DISTINCT I.ID),COUNT(DISTINCT CASE WHEN D.DOC_ID IS "
          "NOT NULL THEN I.ID END),GROUP_CONCAT(DISTINCT "
          "T.LABEL),COUNT(DISTINCT L.TYPE_ID),MIN(I.ID) FROM INTERFACE I LEFT "
          "JOIN INTERFACE_DOCUMENT D ON D.INTERFACE_ID=I.ID LEFT JOIN "
          "INTERFACE_TYPE_LINK L ON L.INTERFACE_ID=I.ID LEFT JOIN "
          "INTERFACE_TYPE T ON T.ID=L.TYPE_ID WHERE COALESCE(I.ARCHIVED,0)=0 "
          "AND ((I.ELEMENT1=? AND I.ELEMENT2=?) OR (I.ELEMENT1=? AND "
          "I.ELEMENT2=?))");
      query.addBindValue(m_ptIds[row]);
      query.addBindValue(m_ptIds[col]);
      query.addBindValue(m_ptIds[col]);
      query.addBindValue(m_ptIds[row]);
      query.exec();
      query.next();
      const int total = query.value(0).toInt(),
                covered = query.value(1).toInt(),
                types = query.value(3).toInt();
      if (total) {
        QString text = query.value(2).toString().isEmpty()
                           ? "Interface"
                           : query.value(2).toString();
        if (total > 1)
          text += QString(" ×%1").arg(total);
        if (types > 1)
          text += QString(" •%1").arg(types);
        item->setText(text);
        item->setData(Qt::UserRole, query.value(4));
        item->setToolTip(QString("%1 interface(s), %2 couverte(s), %3 type(s)")
                             .arg(total)
                             .arg(covered)
                             .arg(types));
        item->setBackground(covered == total ? QColor("#bfe5c8")
                                             : QColor("#ffd59a"));
        if (row < col) {
          all += total;
          coveredAll += covered;
        }
      }
    }
  }
  m_matrix->horizontalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_matrix->verticalHeader()->setSectionResizeMode(
      QHeaderView::ResizeToContents);
  m_rate->setText(QString("Couverture ICD : %1 / %2 interface(s), soit %3 %")
                      .arg(coveredAll)
                      .arg(all)
                      .arg(all ? 100. * coveredAll / all : 0., 0, 'f', 2));
}

void N2MatrixWidget::showDetails(int row, int column) {
  if (row < 0 || column < 0 || row == column || row >= m_ptIds.size() ||
      column >= m_ptIds.size())
    return;
  emit pairSelected(m_ptIds[row], m_ptIds[column]);
  QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
  query.prepare(
      "SELECT I.ID,COALESCE(I.CODE,'IF-'||I.ID),COALESCE(GROUP_CONCAT(DISTINCT "
      "T.LABEL),'Type non défini'),COALESCE(GROUP_CONCAT(DISTINCT "
      "DOC.TITLE),'Aucun ICD'),I.DESCRIPTION FROM INTERFACE I LEFT JOIN "
      "INTERFACE_TYPE_LINK L ON L.INTERFACE_ID=I.ID LEFT JOIN INTERFACE_TYPE T "
      "ON T.ID=L.TYPE_ID LEFT JOIN INTERFACE_DOCUMENT D ON D.INTERFACE_ID=I.ID "
      "LEFT JOIN DOCUMENT DOC ON DOC.ID=D.DOC_ID WHERE "
      "COALESCE(I.ARCHIVED,0)=0 AND ((I.ELEMENT1=? AND I.ELEMENT2=?) OR "
      "(I.ELEMENT1=? AND I.ELEMENT2=?)) GROUP BY I.ID ORDER BY I.CODE");
  query.addBindValue(m_ptIds[row]);
  query.addBindValue(m_ptIds[column]);
  query.addBindValue(m_ptIds[column]);
  query.addBindValue(m_ptIds[row]);
  query.exec();
  QStringList details;
  QList<int> ids;
  while (query.next()) {
    ids << query.value(0).toInt();
    details << QString("%1\nType : %2\nICD : %3\n%4")
                   .arg(query.value(1).toString(), query.value(2).toString(),
                        query.value(3).toString(), query.value(4).toString());
  }
  if (ids.size() == 1) {
    emit interfaceRequested(ids.first());
    return;
  }
  if (details.isEmpty())
    details << "Aucune interface déclarée entre ces éléments.";
  QMessageBox::information(this, "Détail de la cellule N²",
                           details.join("\n\n"));
}
