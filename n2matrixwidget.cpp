#include "n2matrixwidget.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

N2MatrixWidget::N2MatrixWidget(QWidget *parent):QWidget(parent)
{
    m_onlyWithInterfaces=new QCheckBox("Masquer les éléments sans interface",this);
    m_refresh=new QPushButton("Actualiser",this);
    m_matrix=new QTableWidget(this);
    m_matrix->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_matrix->setSelectionMode(QAbstractItemView::SingleSelection);
    QHBoxLayout *tools=new QHBoxLayout;tools->addWidget(new QLabel("Vert : couvert par un ICD — Orange : interface sans ICD",this));tools->addStretch();tools->addWidget(m_onlyWithInterfaces);tools->addWidget(m_refresh);
    QVBoxLayout *layout=new QVBoxLayout(this);layout->addLayout(tools);layout->addWidget(m_matrix);
    connect(m_refresh,&QPushButton::clicked,this,&N2MatrixWidget::refresh);
    connect(m_onlyWithInterfaces,&QCheckBox::toggled,this,&N2MatrixWidget::refresh);
    connect(m_matrix,&QTableWidget::cellDoubleClicked,this,&N2MatrixWidget::showDetails);
}

void N2MatrixWidget::setConnectionName(const QString &connectionName){m_connectionName=connectionName;refresh();}

void N2MatrixWidget::refresh()
{
    QSqlDatabase db=QSqlDatabase::database(m_connectionName);m_matrix->clear();m_ptIds.clear();if(!db.isOpen()){m_matrix->setRowCount(0);m_matrix->setColumnCount(0);return;}
    QString condition;
    if(m_onlyWithInterfaces->isChecked())condition=" WHERE EXISTS(SELECT 1 FROM INTERFACE I WHERE I.ELEMENT1=PT.ID OR I.ELEMENT2=PT.ID)";
    QSqlQuery pt("SELECT ID,NAME FROM PT"+condition+" ORDER BY NAME",db);QStringList names;
    while(pt.next()){m_ptIds<<pt.value(0).toInt();names<<pt.value(1).toString();}
    m_matrix->setRowCount(names.size());m_matrix->setColumnCount(names.size());m_matrix->setHorizontalHeaderLabels(names);m_matrix->setVerticalHeaderLabels(names);
    for(int row=0;row<m_ptIds.size();++row){
        for(int col=0;col<m_ptIds.size();++col){
            QTableWidgetItem *item=new QTableWidgetItem;item->setTextAlignment(Qt::AlignCenter);m_matrix->setItem(row,col,item);
            if(row==col){item->setText("—");item->setBackground(QColor("#dedede"));continue;}
            if(col<row){item->setBackground(QColor("#f5f5f5"));continue;}
            QSqlQuery query(db);query.prepare("SELECT COUNT(DISTINCT I.ID),COUNT(DISTINCT D.INTERFACE_ID),GROUP_CONCAT(DISTINCT T.LABEL) FROM INTERFACE I LEFT JOIN INTERFACE_DOCUMENT D ON D.INTERFACE_ID=I.ID LEFT JOIN INTERFACE_TYPE_LINK L ON L.INTERFACE_ID=I.ID LEFT JOIN INTERFACE_TYPE T ON T.ID=L.TYPE_ID WHERE (I.ELEMENT1=? AND I.ELEMENT2=?) OR (I.ELEMENT1=? AND I.ELEMENT2=?)");
            query.addBindValue(m_ptIds[row]);query.addBindValue(m_ptIds[col]);query.addBindValue(m_ptIds[col]);query.addBindValue(m_ptIds[row]);query.exec();query.next();
            const int total=query.value(0).toInt(),covered=query.value(1).toInt();
            if(total){item->setText(query.value(2).toString().isEmpty()?QString::number(total):query.value(2).toString());item->setToolTip(QString("%1 interface(s), %2 couverte(s)").arg(total).arg(covered));item->setBackground(covered==total?QColor("#bfe5c8"):QColor("#ffd59a"));}
        }
    }
    m_matrix->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);m_matrix->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void N2MatrixWidget::showDetails(int row,int column)
{
    if(row<0||column<0||row==column||row>=m_ptIds.size()||column>=m_ptIds.size())return;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));query.prepare("SELECT COALESCE(I.CODE,'IF-'||I.ID),COALESCE(GROUP_CONCAT(DISTINCT T.LABEL),'Type non défini'),COALESCE(GROUP_CONCAT(DISTINCT DOC.TITLE),'Aucun ICD'),I.DESCRIPTION FROM INTERFACE I LEFT JOIN INTERFACE_TYPE_LINK L ON L.INTERFACE_ID=I.ID LEFT JOIN INTERFACE_TYPE T ON T.ID=L.TYPE_ID LEFT JOIN INTERFACE_DOCUMENT D ON D.INTERFACE_ID=I.ID LEFT JOIN DOCUMENT DOC ON DOC.ID=D.DOC_ID WHERE (I.ELEMENT1=? AND I.ELEMENT2=?) OR (I.ELEMENT1=? AND I.ELEMENT2=?) GROUP BY I.ID ORDER BY I.CODE");
    query.addBindValue(m_ptIds[row]);query.addBindValue(m_ptIds[column]);query.addBindValue(m_ptIds[column]);query.addBindValue(m_ptIds[row]);query.exec();QStringList details;
    while(query.next())details<<QString("%1\nType : %2\nICD : %3\n%4").arg(query.value(0).toString(),query.value(1).toString(),query.value(2).toString(),query.value(3).toString());
    if(details.isEmpty())details<<"Aucune interface déclarée entre ces éléments.";
    QMessageBox::information(this,"Détail de la cellule N²",details.join("\n\n"));
}
