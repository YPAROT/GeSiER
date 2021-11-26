#include "edittableviewform.h"
#include "ui_edittableviewform.h"
#include <QMessageBox>
#include "edittabledialog.h"


EditTableViewForm::EditTableViewForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EditTableViewForm)
{
    ui->setupUi(this);

    m_deleteRowAction = new QAction(tr("Supprimer la ligne selectionnée..."),ui->editTableViewTableWidget);
    connect(m_deleteRowAction, SIGNAL(triggered(bool)), this, SLOT(deleteRow()));
    ui->editTableViewTableWidget->addAction(m_deleteRowAction);
}

EditTableViewForm::~EditTableViewForm()
{
    delete m_deleteRowAction;
    delete ui;
}


void EditTableViewForm::on_editTableViewTableWidget_cellDoubleClicked(int row, int column)
{
    Q_UNUSED(column)

    EditTableDialog editDialog;

    bool isNewTable;
    if (ui->editTableViewTableWidget->item(row,0)->text().isEmpty())
        isNewTable = true;
    else
        isNewTable = false;

    QString infoLabel;
//    bool groupByRequest = (row == 1);
//    if (row == 1)
//        infoLabel = "Utiliser $BOARD_NAME, $BOARD_MODEL et $BOARD_VERSION dans la requete pour prendre en compte le choix de carte par l'interface graphique.";

//    if (editDialog.execute(m_tableViewManager->nameForTableIndex(row),m_tableViewManager->requestForTableIndex(row),(row >1),infoLabel,groupByRequest))
    if (editDialog.execute(m_tableViewManager->nameForTableIndex(row),m_tableViewManager->requestForTableIndex(row),true,infoLabel,false))
    {        
        m_tableViewManager->setNameAndRequestForTableIndex(row , editDialog.tableName(), editDialog.request() );
        ui->editTableViewTableWidget->item(row,0)->setText(editDialog.tableName());
        ui->editTableViewTableWidget->item(row,1)->setText(editDialog.request());
            emit tableViewUpdated();
    }
    else if (isNewTable)
    {
        ui->editTableViewTableWidget->removeRow(row);
        m_tableViewManager->removeTableIndex(row);
    }
}

void EditTableViewForm::on_addTablePushButton_clicked()
{
    int currentRow = ui->editTableViewTableWidget->rowCount();
    m_tableViewManager->addNewTable("Nouvelle table","");
    ui->editTableViewTableWidget->setRowCount(currentRow + 1);
    QTableWidgetItem* item = new QTableWidgetItem("");
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->editTableViewTableWidget->setItem(currentRow,0,item);
    item = new QTableWidgetItem("");
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->editTableViewTableWidget->setItem(currentRow,1,item);
    on_editTableViewTableWidget_cellDoubleClicked(currentRow,0);
}

void EditTableViewForm::updateTableView()
{
    ui->editTableViewTableWidget->clearContents();

    QStringList tableNames = m_tableViewManager->tablesNames();
    ui->editTableViewTableWidget->setRowCount(tableNames.count());
    int currentRow = 0;

    foreach (QString tableName, tableNames)
    {
        QTableWidgetItem* item = new QTableWidgetItem(tableName);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        ui->editTableViewTableWidget->setItem(currentRow,0,item);
        item = new QTableWidgetItem(m_tableViewManager->requestForTableIndex(currentRow));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        ui->editTableViewTableWidget->setItem(currentRow,1,item);
        currentRow = currentRow + 1;
    }
    emit tableViewUpdated();
}

void EditTableViewForm::deleteRow()
{
    QModelIndexList selectedItems = ui->editTableViewTableWidget->selectionModel()->selectedIndexes();
    if (!selectedItems.isEmpty())
    {
        int row = selectedItems.at(0).row();
        if (row > -1)
        {
        m_tableViewManager->removeTableIndex(selectedItems.at(0).row());
        ui->editTableViewTableWidget->removeRow(selectedItems.at(0).row());
        }
//        else
//        {
//            QMessageBox::information(this,"Suppression de table","La table "+ui->editTableViewTableWidget->item(row,0)->text()+" ne peut pas etre supprimée.");
//        }
    }
    emit tableViewUpdated();
}


