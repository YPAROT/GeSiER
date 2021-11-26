#include "qmysqltableview.h"
#include <QtSql>
#include <QHeaderView>
#include <QScrollBar>

FilterHeader::FilterHeader(Qt::Orientation orientation, QWidget *parent):
    QHeaderView(orientation, parent),
    _editors({}),
    _padding(4)
{
    _timer = new QTimer(this);

    setStretchLastSection(true);
    //setSectionResizeMode(QHeaderView::Stretch);
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignCenter);
    setSortIndicatorShown(false);
    connect(this, &FilterHeader::sectionResized, this, &FilterHeader::adjustPositions);
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &FilterHeader::adjustPositions);
}

void FilterHeader::setFilterBoxes(const int columnCount)
{
    QMap<int, QString> regexpColumns = static_cast<QMySqlTableView*>(parent())->getRegExpColumns();
    for(int i(0); i < columnCount; ++i)
    {
        if(static_cast<QMySqlTableView*>(parent())->getExpcludeColumns().contains(i))
            break;
        QLineEdit *le = new QLineEdit(this);
        le->setPlaceholderText(QString("filtre"));
        if(regexpColumns.contains(i))
            le->setValidator(new QRegExpValidator(QRegExp(regexpColumns.value(i))));

        _editors.append(le);
        connect(le, &QLineEdit::textChanged, [this, i](const QString text)
        {
            _timer->setSingleShot(true);
            _timer->start(1000);
            connect(_timer, &QTimer::timeout, [this, i, text](){
                emit filterTViewTextChanged(i, text);});
        });
    }
    adjustPositions();
}

QSize FilterHeader::sizeHint() const
{
    QSize size = QHeaderView::sizeHint();
    if(!_editors.isEmpty())
    {
        const int height = _editors[0]->sizeHint().height();
        size.setHeight(size.height() + height + _padding);
    }
    return size;
}

void FilterHeader::updateGeometries()
{
    if(!_editors.isEmpty())
    {
        const int height = _editors[0]->sizeHint().height();
        setViewportMargins(0, 0, 0, height + _padding);
    }
    else
        setViewportMargins(0, 0, 0, 0);
    QHeaderView::updateGeometries();
    adjustPositions();
}

void FilterHeader::adjustPositions()
{
    int column = 0;
    foreach(QLineEdit *le, _editors)
    {
        const int height = _editors[column]->sizeHint().height() - 2;
        le->move(sectionPosition(column) - offset(), height + (_padding/5));
        le->resize(sectionSize(column), height);
        ++column;
    }
}

QMySqlTableView::QMySqlTableView(QWidget *parent) : QTableView(parent)
{
    _proxy = new QSortFilterProxyModel;
    _header = new FilterHeader(Qt::Horizontal, this);
    setHorizontalHeader(_header);
    _filterLayout = new QVBoxLayout(this);
    horizontalHeader()->setLayout(_filterLayout);
    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);

    connect(_header, &FilterHeader::filterTViewTextChanged, this, &QMySqlTableView::filterTViewTextChanged);

    m_insertRowAction = new QAction(tr("Ajouter une entrée..."),this);
    connect(m_insertRowAction, SIGNAL(triggered(bool)), this, SLOT(on_insertRowAction_triggered()));

    m_deleteRowAction = new QAction(tr("Supprimer les lignes selectionnées..."),this);
    connect(m_deleteRowAction, SIGNAL(triggered(bool)), this, SLOT(on_deleteRowAction_triggered()));

    setEditable(true);
    setSortingEnabled(true);
    verticalHeader()->setVisible(false);    
}


void QMySqlTableView::setEditable(bool editable)
{
    m_isEditable = editable;
    if (m_isEditable)
    {
        addAction(m_insertRowAction);
        addAction(m_deleteRowAction);
    }
    else
    {
        removeAction(m_insertRowAction);
        removeAction(m_deleteRowAction);
    }
}

void QMySqlTableView::filterTViewTextChanged(const int column, const QString pattern)
{
    qDebug() << _proxy << pattern << column;
    _proxy->setFilterKeyColumn(column);
    _proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    _proxy->setFilterFixedString(pattern);
}



void QMySqlTableView::createAction(QAction* action, QString label, const char *member)
{
    action = new QAction(label, this);
    action->setStatusTip(label);
    connect(action, SIGNAL(triggered(bool)), this, member);
}


void QMySqlTableView::updateActions()
{
    QSqlTableModel * tm = qobject_cast<QSqlTableModel *>(model());
    bool enableIns = tm;
    bool enableDel = enableIns && currentIndex().isValid();

//    m_insertRowAction->setEnabled(enableIns);
//    m_deleteRowAction->setEnabled(enableDel);
}

void QMySqlTableView::insertRow()
{
    QSqlQueryModel *queryModel = new QSqlQueryModel(this);
    queryModel->setQuery(QSqlQuery(m_defaultInsertQuery, m_db));

    refreshTable();

    QSqlTableModel *tableModel = qobject_cast<QSqlTableModel *>(model());
    if (!tableModel)
        return;

    edit(tableModel->index(tableModel->rowCount()-1,qMin(tableModel->columnCount()-1,1)));
    doItemsLayout(); // HACK
}

void QMySqlTableView::deleteRow()
{
    QSqlTableModel *_model = qobject_cast<QSqlTableModel *>(model());
    if (!_model)
        return;

    QModelIndexList currentSelection = selectionModel()->selectedIndexes();
    for (int i = 0; i < currentSelection.count(); ++i) {
        if (currentSelection.at(i).column() != 0)
            continue;
        _model->removeRow(currentSelection.at(i).row());
    }
    refreshTable();
    updateActions();
}

QStringList QMySqlTableView::selectedRowsString(int columnIndex)
{
    QSqlTableModel *_model = qobject_cast<QSqlTableModel *>(model());
    if (!_model)
        return QStringList();

    QStringList itemsStr;
    QModelIndexList currentSelection = selectionModel()->selectedIndexes();
    for (int i = 0; i < currentSelection.count(); ++i) {
        if (currentSelection.at(i).column() == columnIndex)
            itemsStr.append(currentSelection.at(i).data().toString());
    }

    return itemsStr;
}

void QMySqlTableView::showTable(const QString &tableName, QSqlDatabase db, QString defaultInsertQuery)
{
    m_defaultInsertQuery = defaultInsertQuery;
    m_db = db;
    m_tableName = tableName;

    refreshTable();
}

void QMySqlTableView::refreshTable()
{
    QSqlTableModel *model = new CustomModel(this, m_db);
    model->setEditStrategy(QSqlTableModel::OnRowChange);
    model->setTable(m_db.driver()->escapeIdentifier(m_tableName, QSqlDriver::TableName));
    model->select();
    if (model->lastError().type() != QSqlError::NoError)
        emit statusMessage(model->lastError().text());
    setUpdatesEnabled(false);
    //   setModel(model);

    setEditTriggers(QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed);
    connect(selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &QMySqlTableView::updateActions);

    _proxy->setSourceModel(model);
    setModel(_proxy);
    resizeRowsToContents();
    resizeColumnsToContents();
    horizontalHeader()->setStretchLastSection(true);
    setUpdatesEnabled(true);
    setEditable(true);
    updateActions();
    doItemsLayout(); // HACK
}


void QMySqlTableView::execQuery(QString queryStr, QSqlDatabase db)
{
    m_db = db;
    QSqlQueryModel *model = new QSqlQueryModel(this);
    model->setQuery(QSqlQuery(queryStr, db));
    //   setModel(model);

    if (model->lastError().type() != QSqlError::NoError)
        emit statusMessage(model->lastError().text());
    else if (model->query().isSelect())
        emit statusMessage(tr("Query OK."));
    else
        emit statusMessage(tr("Query OK, number of affected rows: %1").arg(
                               model->query().numRowsAffected()));

    _proxy->setSourceModel(model);
    setModel(_proxy);
    resizeColumnsToContents();
    resizeRowsToContents();
    horizontalHeader()->setStretchLastSection(true);
    setSortingEnabled(true);
    setEditable(false);
    doItemsLayout(); // HACK
}

void QMySqlTableView::setHiddenColumns(QStringList hiddenColumnsNames)
{
    QSqlQueryModel *model = new QSqlQueryModel(this);
    for (int column = 0; column < model->columnCount();++column)
    {
        QString headerName = model->headerData(column, Qt::Horizontal).toString();
        setColumnHidden(column, (hiddenColumnsNames.indexOf(headerName) >=0));
    }
}


void QMySqlTableView::setModel(QAbstractItemModel *model)
{
    _model = model;
    _header->setFilterBoxes(_model->columnCount());
    QTableView::setModel(model);
}

void QMySqlTableView::setRegExpForColumn(QMap<int, QString> regexpColumns)
{
    _regExpColumns = regexpColumns;
}

void QMySqlTableView::setEditColumns(QVector<int> editColumns)
{
    _expcludeColumns = editColumns;
}

QMap<int, QString> QMySqlTableView::getRegExpColumns()
{
    return _regExpColumns;
}

QVector<int> QMySqlTableView::getExpcludeColumns()
{
    return _expcludeColumns;
}
