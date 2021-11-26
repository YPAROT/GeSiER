#include "importrequirementdialog.h"
#include "ui_importrequirementdialog.h"
#include "qwidgetsettings.h"

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

ImportRequirementDialog::ImportRequirementDialog(QStringList CSVHeaders, QWidget *parent) : QDialog(parent),ui(new Ui::ImportRequirementDialog)
{
    ui->setupUi(this);

    //la check box chapitre n'est plus utilisée au final
    ui->ChapGencheckBox->setHidden(true);

    setWindowTitle("Importer des Requirements");

    ui->PTcomboBox->addItems(CSVHeaders);
    int index = QWidgetSettings::value(QString("ReqImport_%1").arg(PT_INDEX),"").toInt();
    if(index>0)
        ui->PTcomboBox->setCurrentIndex(index);

    ui->ReqCodecomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(CODE_INDEX),"").toInt();
    if(index>0)
        ui->ReqCodecomboBox->setCurrentIndex(index);

    ui->DoccomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(DOC_ID_INDEX),"").toInt();
    if(index>0)
        ui->DoccomboBox->setCurrentIndex(index);

    ui->ChaptercomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(DOC_CHAPTER_INDEX),"").toInt();
    if(index>0)
        ui->ChaptercomboBox->setCurrentIndex(index);

    ui->TitlecomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(TITLE_INDEX),"").toInt();
    if(index>0)
        ui->TitlecomboBox->setCurrentIndex(index);

    ui->DescriptioncomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(DESCRIPTION_INDEX),"").toInt();
    if(index>0)
        ui->DescriptioncomboBox->setCurrentIndex(index);

    ui->TypecomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(TYPE_INDEX),"").toInt();
    if(index>0)
        ui->TypecomboBox->setCurrentIndex(index);

    ui->StatuscomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(STATUS_INDEX),"").toInt();
    if(index>0)
        ui->StatuscomboBox->setCurrentIndex(index);

    ui->SourcecomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(SOURCE_INDEX),"").toInt();
    if(index>0)
        ui->SourcecomboBox->setCurrentIndex(index);

    ui->ParentCodecomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(PARENT_ID_INDEX),"").toInt();
    if(index>0)
        ui->ParentCodecomboBox->setCurrentIndex(index);

    ui->VerifLvlcomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(VERIF_LEVEL_INDEX),"").toInt();
    if(index>0)
        ui->VerifLvlcomboBox->setCurrentIndex(index);

    ui->MethodcomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(VERIF_METHOD_INDEX),"").toInt();
    if(index>0)
        ui->MethodcomboBox->setCurrentIndex(index);

    ui->CommentscomboBox->addItems(CSVHeaders);
    index = QWidgetSettings::value(QString("ReqImport_%1").arg(COMMENTS_INDEX),"").toInt();
    if(index>0)
        ui->CommentscomboBox->setCurrentIndex(index);


    ui->PTfromCodecheckBox->setChecked(QWidgetSettings::value("ReqImport_PTcombostate",false).toBool());
    ui->lineEdit->setText(QWidgetSettings::value("ReqImport_separator","-").toString());


}

ImportRequirementDialog::~ImportRequirementDialog()
{
    delete ui;
}

QMap<int, int> ImportRequirementDialog::getCSVMapping()
{
    QMap<int,int> csvMap;

    csvMap.clear();

    csvMap.insert(PT_INDEX,ui->PTcomboBox->currentIndex());
    csvMap.insert(CODE_INDEX,ui->ReqCodecomboBox->currentIndex());
    csvMap.insert(DOC_ID_INDEX,ui->DoccomboBox->currentIndex());
    csvMap.insert(DOC_CHAPTER_INDEX,ui->ChaptercomboBox->currentIndex());
    csvMap.insert(TITLE_INDEX,ui->TitlecomboBox->currentIndex());
    csvMap.insert(DESCRIPTION_INDEX,ui->DescriptioncomboBox->currentIndex());
    csvMap.insert(TYPE_INDEX,ui->TypecomboBox->currentIndex());
    csvMap.insert(STATUS_INDEX,ui->StatuscomboBox->currentIndex());
    csvMap.insert(SOURCE_INDEX,ui->SourcecomboBox->currentIndex());
    csvMap.insert(PARENT_ID_INDEX,ui->ParentCodecomboBox->currentIndex());
    csvMap.insert(VERIF_LEVEL_INDEX,ui->VerifLvlcomboBox->currentIndex());
    csvMap.insert(VERIF_METHOD_INDEX,ui->MethodcomboBox->currentIndex());
    csvMap.insert(COMMENTS_INDEX,ui->CommentscomboBox->currentIndex());

    return csvMap;

}

bool ImportRequirementDialog::isPTfromReqCodeChecked()
{
    return ui->PTfromCodecheckBox->isChecked();
}

QString ImportRequirementDialog::getSeparateur()
{
    return ui->lineEdit->text();
}

bool ImportRequirementDialog::isChapGenChecked()
{
    return ui->ChapGencheckBox->isChecked();
}

void ImportRequirementDialog::accept()
{
    QMap<int,int> csvMap=getCSVMapping();
    QMap<int,int>::iterator it;

    for(it=csvMap.begin();it!=csvMap.end();it++)
    {
        QWidgetSettings::setValue(QString("ReqImport_%1").arg(it.key()),it.value());
    }

    QWidgetSettings::setValue("ReqImport_PTcombostate",isPTfromReqCodeChecked());
    QWidgetSettings::setValue("ReqImport_separator",getSeparateur());

    QDialog::accept();
}
