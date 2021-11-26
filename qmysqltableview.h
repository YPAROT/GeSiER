#ifndef QMYSQLTABLEVIEW_H
#define QMYSQLTABLEVIEW_H

#include <QObject>
#include <QTableView>
#include <QAction>
#include <QSqlTableModel>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QLineEdit>
#include <QHeaderView>
#include <QSortFilterProxyModel>

class FilterHeader : public QHeaderView
{
    Q_OBJECT
public:
    explicit FilterHeader(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setFilterBoxes(const int columnCount);

    QSize sizeHint() const          Q_DECL_OVERRIDE;

protected:
    void updateGeometries()         Q_DECL_OVERRIDE;

private:
    void adjustPositions();

private:
    QVector<QLineEdit*> _editors;
    int _padding;
    QTimer *_timer;

signals:
    void filterTViewTextChanged(const int column, const QString text);
};



class QMySqlTableView : public QTableView
{
    Q_OBJECT
public:
    explicit QMySqlTableView(QWidget *parent = nullptr);

    void setEditable(bool editable);
    void execQuery(QString queryStr, QSqlDatabase db);
    void updateActions();
    void insertRow();
    void deleteRow();
    void showTable(const QString &t, QSqlDatabase db, QString defaultInsertQuery);
    QStringList selectedRowsString(int columnIndex);
    void setRegExpForColumn(QMap<int, QString> regexpColumns);
    void setEditColumns(QVector<int> editColumns);

    QMap<int, QString>  getRegExpColumns();
    QVector<int>        getExpcludeColumns();

    void setModel(QAbstractItemModel *model) Q_DECL_OVERRIDE;

    QMap<int, QString> *getGetRegEpColumns() const;
    void setGetRegEpColumns(QMap<int, QString> *value);

protected:



private slots:
    void on_insertRowAction_triggered()
    { insertRow(); }
    void on_deleteRowAction_triggered()
    { deleteRow(); }
    void filterTViewTextChanged(const int column, const QString pattern);


signals:
    void statusMessage(const QString &message);
private:
    QAction *m_insertRowAction;
    QAction *m_deleteRowAction;
    bool m_isEditable;

    QString m_defaultInsertQuery;
    QSqlDatabase m_db;
    QString m_tableName;

    void createAction(QAction* action, QString label, const char *member);
    void refreshTable();
    void setHiddenColumns(QStringList hiddenColumnsNames);

    QVBoxLayout *_filterLayout;
    QAbstractItemModel *_model;
    QMap<int, QString> _regExpColumns;
    QVector<int> _expcludeColumns;
    FilterHeader *_header;
    QSortFilterProxyModel * _proxy;
};

class CustomModel: public QSqlTableModel
{
    Q_OBJECT
public:
    explicit CustomModel(QObject *parent = nullptr, QSqlDatabase db = QSqlDatabase())
        : QSqlTableModel(parent, db) {}

    QVariant data(const QModelIndex &idx, int role) const override
    {
        if (role == Qt::BackgroundRole && isDirty(idx))
            return QBrush(QColor(Qt::yellow));
        return QSqlTableModel::data(idx, role);
    }
};

#endif // QMYSQLTABLEVIEW_H
