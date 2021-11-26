#include "importbasicdialog.h"
#include "ui_importbasicdialog.h"
#include <QComboBox>
#include "qwidgetsettings.h"

ImportBasicDialog::ImportBasicDialog(QString tableName,QStringList TableHeaders, QStringList CSVHeaders, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImportBasicDialog)
{
    ui->setupUi(this);
    m_tableName=tableName;
    setWindowTitle("Import des données de la table: "+tableName);
    for(int i=0;i<TableHeaders.count();i++)
    {
        QComboBox * newCombo = new QComboBox(this);
        newCombo->addItems(CSVHeaders);
        int index = QWidgetSettings::value(QString("%1Import_%2").arg(m_tableName).arg(i),"").toInt();
        if(index>0)
            newCombo->setCurrentIndex(index);
        ui->formLayout->addRow(TableHeaders.at(i),newCombo);
    }
}

ImportBasicDialog::~ImportBasicDialog()
{
    delete ui;
}


QMap<int, int> ImportBasicDialog::getCSVMapping()
{
    QMap<int,int> csvMap;

    csvMap.clear();

    for(int i=0;i<ui->formLayout->rowCount();i++)
    {
        QComboBox *currentCombo=qobject_cast<QComboBox*>(ui->formLayout->itemAt(i,QFormLayout::FieldRole)->widget());
        if(currentCombo)
            csvMap.insert(i,currentCombo->currentIndex());
    }


    return csvMap;

}


void ImportBasicDialog::accept()
{
    QMap<int,int> csvMap=getCSVMapping();
    QMap<int,int>::iterator it;

    for(it=csvMap.begin();it!=csvMap.end();it++)
    {
        QWidgetSettings::setValue(QString("%1Import_%2").arg(m_tableName).arg(it.key()),it.value());
    }

    QDialog::accept();
}
