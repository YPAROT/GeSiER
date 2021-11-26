#include "exportcsvdialog.h"
#include "ui_exportcsvdialog.h"
#include <qwidgetsettings.h>
#include <QComboBox>

exportCsvDialog::exportCsvDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::exportCsvDialog)
{
    ui->setupUi(this);
}

exportCsvDialog::~exportCsvDialog()
{
    delete ui;
}


int exportCsvDialog::exec(QStringList tableHeaders, QString tableName)
{
    m_tableName = tableName;
    ui->columnsTableWidget->setRowCount(tableHeaders.count());    
    tableHeaders.insert(0,"Empty");

    for (int row = 0; row < tableHeaders.count(); ++row)
    {        
        ui->columnsTableWidget->setItem(row,0,new QTableWidgetItem(""));
        QComboBox *comboBox = new QComboBox();
        comboBox->addItems(tableHeaders);
        int index = tableHeaders.indexOf(QWidgetSettings::value(m_tableName+"_toCSV_colIndex_"+QString::number(row),"").toString());
        if (index >= 0)
            comboBox->setCurrentIndex(index);
        else
            comboBox->setCurrentIndex(row + 1);

        ui->columnsTableWidget->setItem(row,1,new QTableWidgetItem(QWidgetSettings::value(m_tableName+"_toCSV_colName_"+QString::number(row),"").toString()));

        ui->columnsTableWidget->setCellWidget(row,0,comboBox);
    }

    ui->columnsTableWidget->resizeColumnsToContents();
    ui->columnsTableWidget->horizontalHeader()->stretchLastSection();
    ui->columnsTableWidget->doItemsLayout();

    return QDialog::exec();
}


void exportCsvDialog::accept()
{
    for (int row = 0; row < ui->columnsTableWidget->rowCount(); ++row)
    {
        QComboBox *comboBox= qobject_cast<QComboBox *>(ui->columnsTableWidget->cellWidget(row,0));
        if (comboBox)
            QWidgetSettings::setValue(m_tableName+"_toCSV_colIndex_"+QString::number(row),comboBox->currentText());
        QWidgetSettings::setValue(m_tableName+"_toCSV_colName_"+QString::number(row),ui->columnsTableWidget->item(row,1)->text());
    }

    QDialog::accept();
}


QList<int> exportCsvDialog::columnIndexes()
{
    QList<int> correspondances;
    for (int row = 0; row < ui->columnsTableWidget->rowCount(); ++row)
    {
        QComboBox *comboBox= qobject_cast<QComboBox *>(ui->columnsTableWidget->cellWidget(row,0));
        if (comboBox)
        {
            if (comboBox->currentIndex() > 0)
                correspondances << comboBox->currentIndex() - 1;
        }
    }
    return correspondances;
}

QStringList exportCsvDialog::columnNames()
{
    QStringList correspondances;
    for (int row = 0; row < ui->columnsTableWidget->rowCount(); ++row)
    {
        QComboBox *comboBox= qobject_cast<QComboBox *>(ui->columnsTableWidget->cellWidget(row,0));
        if (comboBox)
        {
            if (comboBox->currentIndex() > 0)
            {
                if (ui->columnsTableWidget->item(row,1)->text().isEmpty())
                    correspondances << comboBox->currentText();
                else
                    correspondances << ui->columnsTableWidget->item(row,1)->text();
            }
        }
    }
    return correspondances;
}
