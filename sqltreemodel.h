#ifndef SQLTREEMODEL_H
#define SQLTREEMODEL_H

#include <QtSql>
#include <QAbstractItemModel>
#include "sqltreeitem.h"
#include <QMap>

class SqlTreeModel:public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit SqlTreeModel(const QString DBConnectionName,QObject *parent = Q_NULLPTR);
    ~SqlTreeModel();

    bool setRelation(QSqlRelation relation);

    bool select();

    void setColumnMapping(QMap<int,int> columnMap);


    //override
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,int role = Qt::DisplayRole) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role = Qt::EditRole) override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;


private:
    QSqlRelation m_relation;
    SqlTreeItem * m_rootItem;
    QString m_dbConnectionName;
    QMap<int,int> m_columnMap;



};

#endif // SQLTREE_H
