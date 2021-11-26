#ifndef SQLTREEREQMODEL_H
#define SQLTREEREQMODEL_H

#include <QtSql>
#include <QAbstractItemModel>
#include "sqltreeitem.h"
#include <QMap>

class SqlTreeReqModel:public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit SqlTreeReqModel(const QSqlDatabase &db = QSqlDatabase(),QObject *parent = Q_NULLPTR);
    ~SqlTreeReqModel();

    bool setRelation(QSqlRelation relation);

    bool select(QVariant reqID);

    void setColumnMapping(QMap<int,int> columnMap);


    //override
    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;


private:
    QSqlRelation m_relation;
    SqlTreeItem * m_rootItem;
    QSqlDatabase m_db;
    QMap<int,int> m_columnMap;
};

#endif // SQLTREEREQMODEL_H
