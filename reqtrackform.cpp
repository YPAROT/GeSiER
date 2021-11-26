#include "reqtrackform.h"
#include "ui_reqtrackform.h"
#include <QDebug>
#include <QCompleter>

ReqTrackForm::ReqTrackForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ReqTrackForm)
{
    ui->setupUi(this);
    m_req_model=Q_NULLPTR;
    m_Req_Arbo=Q_NULLPTR;

    ui->ReqcomboBox->setInsertPolicy(QComboBox::NoInsert);
    ui->ReqcomboBox->setEditable(true);
    ui->ReqcomboBox->completer()->setFilterMode(Qt::MatchContains);
    ui->ReqcomboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);
}

ReqTrackForm::~ReqTrackForm()
{
    delete ui;
    delete m_req_model;
    delete m_Req_Arbo;
}

void ReqTrackForm::setConnectionName(QString connectionName)
{
    m_connectionName = connectionName;
}

bool ReqTrackForm::init()
{
    if(!QSqlDatabase::database(m_connectionName).isValid())
        return false;

    fillComboBox(QSqlDatabase::database(m_connectionName));
    SetModel(m_req_model->index(ui->ReqcomboBox->currentIndex(),m_req_model->fieldIndex("ID")),QSqlDatabase::database(m_connectionName));

    return true;
}

bool ReqTrackForm::refresh()
{
    QString oldSelectedReq=ui->ReqcomboBox->currentText();

    fillComboBox(QSqlDatabase::database(m_connectionName));

    if(!m_req_model)
        return false;

    QModelIndexList idx=m_req_model->match(m_req_model->index(0,m_req_model->fieldIndex("CODE")),Qt::DisplayRole,QVariant(oldSelectedReq));

    if(idx.isEmpty())
        return false;


    if(!m_Req_Arbo)
        return false;

    if(!idx.at(0).isValid())
    {
//        m_Req_Arbo->select(m_req_model->index(ui->ReqcomboBox->currentIndex(),m_req_model->fieldIndex("ID")).data());
//        ui->treeView->expandAll();
        qDebug()<<"Refresh non possible en conservant l'ancienne selection combo box (ReqTrackForm::refresh)";
        return false;
    }

    ui->ReqcomboBox->setCurrentIndex(idx.at(0).row());

    qDebug()<<"Refresh effectué sur ID: "<<m_req_model->index(idx.at(0).row(),m_req_model->fieldIndex("ID")).data()<<" (ReqTrackForm::refresh)";

    return true;
}


bool ReqTrackForm::fillComboBox(QSqlDatabase db)
{
    if(!db.isOpen())
        return false;

    if(m_req_model)
    {
        delete m_req_model;
    }

    m_req_model = new QSqlTableModel(this,db);
    m_req_model->setTable("REQUIREMENT");
    m_req_model->select();

    ui->ReqcomboBox->setModel(m_req_model);
    ui->ReqcomboBox->setModelColumn(m_req_model->fieldIndex("CODE"));
//    ui->ReqcomboBox->setEditable(true);

    return true;
}

bool ReqTrackForm::SetModel(QModelIndex idx,QSqlDatabase db)
{
    if(!db.isOpen())
        return false;

    if(!idx.isValid())
        return false;

    if(!m_Req_Arbo)
        delete m_Req_Arbo;

    m_Req_Arbo = new SqlTreeReqModel(db,this);

    QMap<int,int> ReqArbomap;
    ReqArbomap.insert(0,1);
    ReqArbomap.insert(1,2);
    ReqArbomap.insert(2,0);
    m_Req_Arbo->setColumnMapping(ReqArbomap);

    qDebug()<<"ID selectionné: "<<idx.data()<<" (ReqTrackForm::SetModel)";
    m_Req_Arbo->select(idx.data());

    ui->treeView->setModel(m_Req_Arbo);

    ui->treeView->hideColumn(2);
    ui->treeView->hideColumn(3);
    ui->treeView->hideColumn(4);
    ui->treeView->expandAll();
    ui->treeView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    return true;
}

void ReqTrackForm::on_ReqcomboBox_currentIndexChanged(int index)
{
    if(!m_req_model)
        return;
    QModelIndex idx=m_req_model->index(index,m_req_model->fieldIndex("ID"));
    if(!idx.isValid())
        return;
    if(!m_Req_Arbo)
        return;
    m_Req_Arbo->select(idx.data());
    ui->treeView->expandAll();
    qDebug()<<"ID selectionné: "<<idx.data()<<" (ReqTrackForm::on_ReqcomboBox_currentIndexChanged)";
//    SetModel(idx,QSqlDatabase::database(m_connectionName));
}
