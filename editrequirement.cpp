#include "editrequirement.h"
#include "ui_editrequirement.h"
#include <QDebug>
//#include <QSqlRelationalDelegate>
#include "sqlproxyrelationaldelegate.h"
#include <QCompleter>


#define PT_INDEX 1
#define CODE_INDEX 2
#define DOC_ID_INDEX 3
#define DOC_CHAPTER_INDEX 4
#define TITLE_INDEX 5
#define DESCRIPTION_INDEX 6
#define TYPE_INDEX 7
#define STATUS_INDEX 8
#define SOURCE_INDEX 9
#define PARENT_ID_INDEX 10
#define VERIF_LEVEL_INDEX 11
#define VERIF_METHOD_INDEX 12
#define COMMENTS_INDEX 13


EditRequirement::EditRequirement(QAbstractItemModel *model, const QModelIndex &index, QWidget *parent):QDialog(parent),ui(new Ui::EditRequirement)
{

    QSqlRelationalTableModel *sqlModel=NULL;
    QModelIndex newIndex=index;

    ui->setupUi(this);

    this->setWindowTitle("Requirements Editor");

    //Attention aux proxy!!! il faut mapper
    QSortFilterProxyModel *proxy = qobject_cast<QSortFilterProxyModel *>(model);
    if(proxy)
    {
        newIndex=proxy->mapToSource(index);
        while(qobject_cast<QSortFilterProxyModel *>(proxy->sourceModel()))
        {
            proxy = qobject_cast<QSortFilterProxyModel *>(proxy->sourceModel());
            newIndex=proxy->mapToSource(newIndex);
        }

        sqlModel = qobject_cast<QSqlRelationalTableModel *>(proxy->sourceModel());
    }
    else
    {
        sqlModel = qobject_cast<QSqlRelationalTableModel *>(model);
    }

    //initialisation des combobox
    ui->PT_comboBox->setModel(sqlModel->relationModel(PT_INDEX));
    ui->PT_comboBox->setModelColumn(sqlModel->relationModel(PT_INDEX)->fieldIndex("NAME"));
//    ui->PT_comboBox->setInsertPolicy(QComboBox::NoInsert);
//    ui->PT_comboBox->setEditable(true);
//    ui->PT_comboBox->completer()->setFilterMode(Qt::MatchContains);
//    ui->PT_comboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);

    //ajout proxy filtre
    m_reqDocProxy=new QSortFilterProxyModel(this);
    m_reqDocProxy->setSourceModel(sqlModel->relationModel(DOC_ID_INDEX));
    m_reqDocProxy->setFilterKeyColumn(2);//filtre sur Doc type
    m_reqDocProxy->setFilterFixedString("1"); //filtre sur type = SP... en dur ici mais à retravailler.
    ui->DocID_comboBox->setModel(m_reqDocProxy);
    ui->DocID_comboBox->setModelColumn(3);
//    ui->DocID_comboBox->setModel(sqlModel->relationModel(DOC_ID_INDEX));
//    ui->DocID_comboBox->setModelColumn(sqlModel->relationModel(DOC_ID_INDEX)->fieldIndex("TITLE"));
//    ui->DocID_comboBox->setInsertPolicy(QComboBox::NoInsert);
//    ui->DocID_comboBox->setEditable(true);
//    ui->DocID_comboBox->completer()->setFilterMode(Qt::MatchContains);
//    ui->DocID_comboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);


    //ajout proxy filtre
    m_chapterProxy=new QSortFilterProxyModel(this);
    m_chapterProxy->setSourceModel(sqlModel->relationModel(DOC_CHAPTER_INDEX));
    m_chapterProxy->setFilterKeyColumn(1);//filtre sur DOC_ID
    ui->DocChapter_comboBox->setModel(m_chapterProxy);
    ui->DocChapter_comboBox->setModelColumn(2);
    ui->DocChapter_comboBox->addItem("NULL"); // ! Ajoute une entrée temporaire au modèle !
//    ui->DocChapter_comboBox->setInsertPolicy(QComboBox::NoInsert);
//    ui->DocChapter_comboBox->setEditable(true);
//    ui->DocChapter_comboBox->completer()->setFilterMode(Qt::MatchContains);
//    ui->DocChapter_comboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);

    ui->Type_comboBox->setModel(sqlModel->relationModel(TYPE_INDEX));
    ui->Type_comboBox->setModelColumn(sqlModel->relationModel(TYPE_INDEX)->fieldIndex("TYPE"));
//    ui->Type_comboBox->setInsertPolicy(QComboBox::NoInsert);
//    ui->Type_comboBox->setEditable(true);
//    ui->Type_comboBox->completer()->setFilterMode(Qt::MatchContains);
//    ui->Type_comboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);

    ui->Status_comboBox->setModel(sqlModel->relationModel(STATUS_INDEX));
    ui->Status_comboBox->setModelColumn(sqlModel->relationModel(STATUS_INDEX)->fieldIndex("STATUS"));
//    ui->Status_comboBox->setInsertPolicy(QComboBox::NoInsert);
//    ui->Status_comboBox->setEditable(true);
//    ui->Status_comboBox->completer()->setFilterMode(Qt::MatchContains);
//    ui->Status_comboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);

    ui->Parent_comboBox->setModel(sqlModel->relationModel(PARENT_ID_INDEX));
    ui->Parent_comboBox->setModelColumn(sqlModel->relationModel(PARENT_ID_INDEX)->fieldIndex("CODE"));
    ui->Parent_comboBox->addItem("NULL"); // ! Ajoute une entrée temporaire au modèle !
//    ui->Parent_comboBox->setInsertPolicy(QComboBox::NoInsert);
//    ui->Parent_comboBox->setEditable(true);
//    ui->Parent_comboBox->completer()->setFilterMode(Qt::MatchContains);
//    ui->Parent_comboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);

    ui->VerifMethod_comboBox->setModel(sqlModel->relationModel(VERIF_METHOD_INDEX));
    ui->VerifMethod_comboBox->setModelColumn(sqlModel->relationModel(VERIF_METHOD_INDEX)->fieldIndex("METHOD"));
//    ui->VerifMethod_comboBox->setInsertPolicy(QComboBox::NoInsert);
//    ui->VerifMethod_comboBox->setEditable(true);
//    ui->VerifMethod_comboBox->completer()->setFilterMode(Qt::MatchContains);
//    ui->VerifMethod_comboBox->completer()->setCompletionMode(QCompleter::PopupCompletion);

    //mapping
    m_mapper = new QDataWidgetMapper(this);
    //mapping mode
    m_mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    //model
    m_mapper->setModel(sqlModel);
    m_mapper->setItemDelegate(new SqlProxyRelationalDelegate(this));
    m_mapper->addMapping(ui->PT_comboBox,PT_INDEX);
    m_mapper->addMapping(ui->ReqCode_lineEdit,CODE_INDEX);
    m_mapper->addMapping(ui->DocID_comboBox,DOC_ID_INDEX);
    m_mapper->addMapping(ui->DocChapter_comboBox,DOC_CHAPTER_INDEX);
    m_mapper->addMapping(ui->Title_lineEdit,TITLE_INDEX);
    m_mapper->addMapping(ui->Description_plainTextEdit,DESCRIPTION_INDEX);
    m_mapper->addMapping(ui->Type_comboBox,TYPE_INDEX);
    m_mapper->addMapping(ui->Status_comboBox,STATUS_INDEX);
    m_mapper->addMapping(ui->Source_lineEdit,SOURCE_INDEX);
    m_mapper->addMapping(ui->Parent_comboBox,PARENT_ID_INDEX);
    m_mapper->addMapping(ui->VerifLvl_lineEdit,VERIF_LEVEL_INDEX);
    m_mapper->addMapping(ui->VerifMethod_comboBox,VERIF_METHOD_INDEX);
    m_mapper->addMapping(ui->Comments_plainTextEdit,COMMENTS_INDEX);



    //connections
    connect(ui->DocID_comboBox,SIGNAL(currentIndexChanged(int)),this,SLOT(OnDocIDChange(int)));

    //init
    m_mapper->setCurrentIndex(newIndex.row());  //il y a un pb sur les nouvelles rangées pour les tables liées (combobox)

    //Si des NULL sont positionnés sur les comboBox il faut les remettre
    if(model->data(model->index(index.row(),DOC_CHAPTER_INDEX))=="NULL")
        ui->DocChapter_comboBox->setCurrentIndex(ui->DocChapter_comboBox->count()-1);

    if(model->data(model->index(index.row(),PARENT_ID_INDEX))=="NULL")
        ui->Parent_comboBox->setCurrentIndex(ui->Parent_comboBox->count()-1);


}

EditRequirement::~EditRequirement()
{
    delete ui;
}

void EditRequirement::ApplyFieldsChanges()
{
    m_mapper->submit();
}

void EditRequirement::OnDocIDChange(int index)
{
    QAbstractItemModel *DocTableModel = ui->DocID_comboBox->model();
    QVariant docCurrentID = DocTableModel->data(DocTableModel->index(index,0)); //recup de l'ID en colonne 0
    m_chapterProxy->setFilterFixedString(docCurrentID.toString());
}
