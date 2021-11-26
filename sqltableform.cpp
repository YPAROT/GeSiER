#include "sqltableform.h"
#include "ui_sqltableform.h"
#include <QtSql>
#include <QScrollBar>
#include <QMessageBox>
#include "SqlProxyRelationalDelegate.h"


SQLTableForm::SQLTableForm(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SQLTableForm)
{
    ui->setupUi(this);

    m_lineEditorChangeTimer = new QTimer();
    m_sqlTableModel = Q_NULLPTR;
    ui->sqlTableView->horizontalHeader()->setVisible(false);
    ui->sqlTableView->verticalHeader()->setVisible(false);

    //modif type selection
    //ui->sqlTableView->setSelectionBehavior(QAbstractItemView::SelectItems);

    //modif pour ne pas dépasse un max en resize
    ui->sqlTableView->horizontalHeader()->setMaximumSectionSize(500);

    ui->headerTableWidget->verticalHeader()->setVisible(false);
    ui->headerTableWidget->setVisible(false);
    ui->headerTableWidget->horizontalScrollBar()->setVisible(false);
    ui->headerTableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->headerTableWidget->verticalHeader()->setMaximumHeight(25);
    m_insertRowAction = new QAction(tr("Ajouter une entrée..."),ui->sqlTableView);
    connect(m_insertRowAction, SIGNAL(triggered(bool)), this, SLOT(insertRow()));

    m_deleteRowAction = new QAction(tr("Supprimer les lignes selectionnées..."),ui->sqlTableView);
    connect(m_deleteRowAction, SIGNAL(triggered(bool)), this, SLOT(deleteRow()));

    m_commitAction = new QAction(tr("Commiter les changements..."),ui->sqlTableView);
    connect(m_commitAction, SIGNAL(triggered(bool)), this, SLOT(comitAll()));

    m_revertAction = new QAction(tr("Annuler les changements..."),ui->sqlTableView);
    connect(m_revertAction, SIGNAL(triggered(bool)), this, SLOT(revertAll()));

    m_resizeTableAction = new QAction(tr("Ajuster les lignes/colonnes au contenu..."),ui->sqlTableView);
    connect(m_resizeTableAction, SIGNAL(triggered(bool)), this, SLOT(resizeTable()));

    setEditable(false);
    ui->sqlTableView->setSortingEnabled(true);
    ui->sqlTableView->horizontalHeader()->setStretchLastSection(true);

    connect(m_lineEditorChangeTimer,SIGNAL(timeout()),this,SLOT(lineEditorChanged()));
    connect(ui->sqlTableView->horizontalScrollBar(),SIGNAL(valueChanged(int)),ui->headerTableWidget->horizontalScrollBar(),SLOT(setValue(int)));

    connect(ui->headerTableWidget->horizontalHeader(),SIGNAL(sortIndicatorChanged(int,Qt::SortOrder))
            ,this,SLOT(sortIndicatorChanged(int,Qt::SortOrder)));
}

SQLTableForm::~SQLTableForm()
{
    delete ui;
}

void SQLTableForm::comitAll()
{
    if (m_sqlTableModel)
    {
        if (m_sqlTableModel->submitAll() == false)
            QMessageBox::warning(this,"Comit failure",m_sqlTableModel->lastError().text());
        emit comitOccured();
    }
}

void SQLTableForm::revertAll()
{
    if (m_sqlTableModel)
    {
        m_sqlTableModel->revertAll();
    }
}

void SQLTableForm::resizeTable()
{
    disconnect(ui->headerTableWidget->horizontalHeader(),SIGNAL(sectionResized(int,int,int)),this,SLOT(resultSectionResized(int,int,int)));
    ui->sqlTableView->resizeRowsToContents();
    ui->sqlTableView->resizeColumnsToContents();
    ui->sqlTableView->doItemsLayout(); // HACK

    for (int i=0; i < ui->headerTableWidget->columnCount();i++)
        ui->headerTableWidget->setColumnWidth(i,ui->sqlTableView->columnWidth(i));

    connect(ui->headerTableWidget->horizontalHeader(),SIGNAL(sectionResized(int,int,int)),this,SLOT(resultSectionResized(int,int,int)));
}


void SQLTableForm::sortIndicatorChanged(int column, Qt::SortOrder order)
{
    if (!m_proxies.isEmpty())
        m_proxies.last()->sort(column,order);
}

void SQLTableForm::resultSectionResized(int logicalIndex, int oldSize, int newSize)
{
    Q_UNUSED(logicalIndex)
    Q_UNUSED(oldSize)
    Q_UNUSED(newSize)
    for (int i=0; i < ui->headerTableWidget->columnCount();i++)
        ui->sqlTableView->setColumnWidth(i,ui->headerTableWidget->columnWidth(i));
}


void SQLTableForm::updateHeaders()
{

    if (ui->sqlTableView->model() && (ui->sqlTableView->model()->columnCount() == 0))
        return;

    disconnect(ui->headerTableWidget->horizontalHeader(),SIGNAL(sectionResized(int,int,int)),this,SLOT(resultSectionResized(int,int,int)));

    //affichage headers
    QMap<int,QPair<QVariant,int>>::iterator it;
    for(it=m_headerMap.begin();it!=m_headerMap.end();it++)
    {
        ui->sqlTableView->model()->setHeaderData(it.key(),Qt::Horizontal,it.value().first,it.value().second);
    }

    QStringList headers;
    for(int i = 0; i < ui->sqlTableView->model()->columnCount(); i++)
    {
        headers.append(ui->sqlTableView->model()->headerData(i, Qt::Horizontal).toString());
    }

    ui->headerTableWidget->setVisible(true);
    ui->headerTableWidget->setColumnCount(headers.count());
    ui->headerTableWidget->setRowCount(1);
    ui->headerTableWidget->setHorizontalHeaderLabels(headers);

    //    while (m_lineEditors.count())
    //        delete m_lineEditors.takeLast();

    //    while (m_proxies.count())
    //        delete m_proxies.takeLast();

    m_lineEditors.clear();
    m_proxies.clear();
    for (int i=0; i < headers.count();++i)
    {
        QLineEdit *lineEditor = new QLineEdit(this);
        lineEditor->setPlaceholderText(QString("filtre"));
        m_lineEditors.append(lineEditor);
        connect(lineEditor, SIGNAL(textChanged(QString)),this,SLOT(lineEditorJustChanged()));
        ui->headerTableWidget->setCellWidget(0,i,lineEditor);

        QSortFilterProxyModel *proxy = new QSortFilterProxyModel;
        proxy->setFilterKeyColumn(i);
        proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        if (m_proxies.isEmpty())
            proxy->setSourceModel(ui->sqlTableView->model());
        else
            proxy->setSourceModel(m_proxies.last());
        m_proxies.append(proxy);
    }

    if (!m_proxies.isEmpty())
    {
        ui->sqlTableView->setModel(m_proxies.last());
        ui->sqlTableView->setItemDelegate(new SqlProxyRelationalDelegate(ui->sqlTableView));
    }

    int height = ui->headerTableWidget->horizontalHeader()->height()
            + ui->headerTableWidget->rowHeight(0)+4;

    ui->headerTableWidget->setMaximumHeight(height);
    for (int i=0; i < ui->headerTableWidget->columnCount();i++)
        ui->headerTableWidget->setColumnWidth(i,ui->sqlTableView->columnWidth(i));

    connect(ui->headerTableWidget->horizontalHeader(),SIGNAL(sectionResized(int,int,int)),this,SLOT(resultSectionResized(int,int,int)));
}

void SQLTableForm::setColumnHidden(int column,bool hide)
{
    if (column < ui->headerTableWidget->columnCount())
    {
        ui->headerTableWidget->setColumnHidden(column, hide);
        ui->sqlTableView->setColumnHidden(column, hide);
    }
}
void SQLTableForm::lineEditorJustChanged()
{
    m_lineEditorChangeTimer->start(500);
}

void SQLTableForm::lineEditorChanged()
{
    m_lineEditorChangeTimer->stop();
    // filter here
    for (int i=0; i < m_lineEditors.count();++i)
    {
        m_proxies.at(i)->setFilterFixedString(m_lineEditors.at(i)->text());
    }
}

void SQLTableForm::setEditable(bool editable)
{
    m_isEditable = editable;
    if (m_isEditable)
    {
        ui->sqlTableView->addAction(m_insertRowAction);
        ui->sqlTableView->addAction(m_deleteRowAction);
        ui->sqlTableView->addAction(m_commitAction);
        ui->sqlTableView->addAction(m_revertAction);
    }
    else
    {
        ui->sqlTableView->removeAction(m_insertRowAction);
        ui->sqlTableView->removeAction(m_deleteRowAction);
        ui->sqlTableView->removeAction(m_commitAction);
        ui->sqlTableView->removeAction(m_revertAction);
    }
    ui->sqlTableView->addAction(m_resizeTableAction);

}

void SQLTableForm::insertRow()
{    
    if (!m_sqlTableModel)
        return;

//    ui->sqlTableView->setCurrentIndex(ui->sqlTableView->model()->index(m_sqlTableModel->rowCount()-1, 1));
//    ui->sqlTableView->edit(ui->sqlTableView->model()->index(m_sqlTableModel->rowCount()-1, 1));

    m_sqlTableModel->insertRow(m_sqlTableModel->rowCount());
}

void SQLTableForm::deleteRow()
{
    if (!ui->sqlTableView->model())
        return;

    QModelIndexList currentSelection = ui->sqlTableView->selectionModel()->selectedIndexes();
    for (int i = 0; i < currentSelection.count(); ++i) {
        if (currentSelection.at(i).column() != 0)
            continue;
        ui->sqlTableView->model()->removeRow(currentSelection.at(i).row());
    }
    //refreshTable();
}

QStringList SQLTableForm::selectedRowsString(int columnIndex)
{
    if (!ui->sqlTableView->model())
        return QStringList();

    QStringList itemsStr;
    QModelIndexList currentSelection = ui->sqlTableView->selectionModel()->selectedIndexes();
    for (int i = 0; i < currentSelection.count(); ++i) {
        if (currentSelection.at(i).column() == columnIndex)
            itemsStr.append(currentSelection.at(i).data().toString());
    }

    return itemsStr;
}

void SQLTableForm::showTable(const QString &tableName, QString dbConnectionName)
{
    m_dbConnectionName =  dbConnectionName;
    m_tableName = tableName;

    refreshTable(true);
}

void SQLTableForm::refreshTable(bool newTable)
{
    ui->sqlTableView->setUpdatesEnabled(false);

    if(m_sqlTableModel)
        delete m_sqlTableModel;

    QSqlDatabase db = QSqlDatabase::database(m_dbConnectionName);
    if(!db.isValid())
        return;


    m_sqlTableModel = new CustomModel(ui->sqlTableView, db);
    m_sqlTableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_sqlTableModel->setTable(db.driver()->escapeIdentifier(m_tableName, QSqlDriver::TableName));


    for(int i=0;i<m_relationList.count();i++)
    {
        m_sqlTableModel->setRelation(m_relationList.at(i).first,m_relationList.at(i).second);
    }


    m_sqlTableModel->select();
    if (m_sqlTableModel->lastError().type() != QSqlError::NoError)
        emit statusMessage(m_sqlTableModel->lastError().text());

    if (newTable)
    {
        ui->sqlTableView->setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
        ui->sqlTableView->setModel(m_sqlTableModel);
        setEditable(true);
        updateHeaders();
    }
    else
    {
        if (m_proxies.count())
            m_proxies.first()->setSourceModel(m_sqlTableModel);
    }
    ui->sqlTableView->setUpdatesEnabled(true);
}

void SQLTableForm::addRelationalTable(int colonne, QSqlRelation relation)
{
    m_relationList.append(QPair<int,QSqlRelation>(colonne,relation));
    refreshTable();
}

void SQLTableForm::removeRelationalTable(int index)
{
    m_relationList.removeAt(index);
    refreshTable();
}

void SQLTableForm::removeRelationalTable(QString tableName)
{
    for (int i=0;i<m_relationList.count();i++)
    {
        if(m_relationList.at(i).second.tableName()==tableName)
        {
            m_relationList.removeAt(i);
        }
    }

    refreshTable();
}

QSqlRelation SQLTableForm::getRelationAtColumn(int index)
{
    for (int i=0;i<m_relationList.count();i++)
    {
        if(m_relationList.at(i).first==index)
        {
            return m_relationList.at(i).second;
        }
    }

    return QSqlRelation();
}

void SQLTableForm::SetEditionTriggers(QAbstractItemView::EditTriggers triggers)
{
    ui->sqlTableView->setEditTriggers(triggers);
}

void SQLTableForm::SetHorizontalHeaders(int section, const QVariant &value, int role)
{

    m_headerMap.insert(section,QPair<QVariant,int>(value,role));

}

QStringList SQLTableForm::getHorizontalHeadersNames()
{
    QStringList result;
    QMap<int,QPair<QVariant,int>>::iterator it;
    for(it=m_headerMap.begin();it!=m_headerMap.end();it++)
    {
        result<<it.value().first.toString();
    }

    //si c'est vide, on prends les headers de la table
    if(result.isEmpty())
        return getFieldNames();

    return result;
}

int SQLTableForm::getFieldIndex(const QString fieldName)
{
    return m_sqlTableModel->fieldIndex(fieldName);
}

QStringList SQLTableForm::getFieldNames()
{
    QStringList fieldNames;
    QSqlRecord rec = m_sqlTableModel->record();
    for(int i=0;i<rec.count();i++)
    {
        fieldNames<<rec.fieldName(i);
    }

    return fieldNames;
}

void SQLTableForm::insertRow(QMap<int,QVariant> fieldsValue)
{
    //ajout ligne à la fin
    insertRow();

    //édition
    QMap<int,QVariant>::iterator it;
    for(it=fieldsValue.begin();it!=fieldsValue.end();it++)
    {
        QModelIndex idx = m_sqlTableModel->index(m_sqlTableModel->rowCount()-1,it.key());
        m_sqlTableModel->setData(idx,it.value());
    }
}

void SQLTableForm::updateRow(int row, QMap<int, QVariant> fieldsValue)
{
    QMap<int,QVariant>::iterator it;
    for(it=fieldsValue.begin();it!=fieldsValue.end();it++)
    {
        QModelIndex idx = m_sqlTableModel->index(row,it.key());
        m_sqlTableModel->setData(idx,it.value());
    }
}

int SQLTableForm::findRow(int field, QVariant uniqueParameterValue)
{
    QList<int> rows;
    for(int i=0;i<m_sqlTableModel->rowCount();i++)
    {
        QModelIndex idx = m_sqlTableModel->index(i,field);
        if(m_sqlTableModel->data(idx).toString()==uniqueParameterValue.toString())
        {
            rows<<i;
        }
    }

    if(rows.count()==1)
    {
        return rows.first();
    }

    return -1;

}

QSqlIndex SQLTableForm::primaryKey()
{
    return m_sqlTableModel->primaryKey();
}


QTableView* SQLTableForm::sqlTableView()
{
    return ui->sqlTableView;
}

void SQLTableForm::execQuery(QString queryStr, QString dbConnectionName)
{
    ui->sqlTableView->setUpdatesEnabled(false);
    QSqlDatabase db = QSqlDatabase::database(dbConnectionName);
    if(!db.isValid())
        return;
    QSqlQueryModel *model = new QSqlQueryModel(ui->sqlTableView);
    model->setQuery(QSqlQuery(queryStr, db));

    if (model->lastError().type() != QSqlError::NoError)
        emit statusMessage(model->lastError().text());
    else if (model->query().isSelect())
        emit statusMessage(tr("Query OK."));
    else
        emit statusMessage(tr("Query OK, number of affected rows: %1").arg(
                               model->query().numRowsAffected()));

    ui->sqlTableView->setModel(model);
    ui->sqlTableView->setUpdatesEnabled(true);
    setEditable(false);

    updateHeaders();
}


void SQLTableForm::on_sqlTableView_doubleClicked(const QModelIndex &index)
{

    if(!m_isEditable || !m_sqlTableModel)
        return;

    if(m_sqlTableModel->tableName()=="\"REQUIREMENT\"")
    {
        EditRequirement *edit = new EditRequirement(this->m_proxies.last(),index,this);
        int result=edit->exec();
        if(result==QDialog::Accepted)
        {
            edit->ApplyFieldsChanges();
        }
        delete edit;
    }

}
