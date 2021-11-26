#include "sqltreemodel.h"

#define START_DATA_ROW 1

SqlTreeModel::SqlTreeModel(const QString DBConnectionName, QObject *parent):QAbstractItemModel(parent)
{
    m_dbConnectionName = DBConnectionName;
    m_columnMap.clear();
    m_rootItem = Q_NULLPTR;
}

SqlTreeModel::~SqlTreeModel()
{
    delete m_rootItem;
}

bool SqlTreeModel::setRelation(QSqlRelation relation)
{
    m_relation = relation;
    if (!relation.isValid())
        return false;
    return true;
}

bool SqlTreeModel::select()
{
    QList<SqlTreeItem*> treeItems;

    //Il faut effacer le modèle si il y a déjà des données (on ne garde que le root).
    beginResetModel();
    if(m_rootItem)
    {
        delete m_rootItem;
    }

    // On exécute un select All sur la table et on regarde qui doit matcher avec qui. Par défaut on prend toutes les colonnes de la query
    QSqlQuery query("SELECT * FROM "+m_relation.tableName(),QSqlDatabase::database(m_dbConnectionName));

    //on ajoute les headers
    QList<QVariant> headerfromquery;
    for (int i =0;i<query.record().count();i++)
    {
        headerfromquery.append(query.record().fieldName(i));
    }

    m_rootItem = new SqlTreeItem(headerfromquery);

    if(!query.first())
        return false;

    {
        QSqlRecord rec=query.record();
        QList<QVariant> data;
        for(int i=0;i<rec.count();i++)
        {
            data.append(rec.value(i));
        }
        treeItems.append(new SqlTreeItem(data));
    }
    while (query.next())
    {
        QSqlRecord rec=query.record();
        QList<QVariant> data;
        for(int i=0;i<rec.count();i++)
        {
            data.append(rec.value(i));
        }
        treeItems.append(new SqlTreeItem(data));
    }

    //il reste a éditer les liens parent enfants
    int parent_idx = query.record().indexOf(m_relation.displayColumn());
    int id_idx = query.record().indexOf(m_relation.indexColumn());

    for (int i=0;i<treeItems.count();i++)
    {
        if(treeItems.at(i)->data(parent_idx)!="")
        {
            //il y a un parent on l'y relie
            for(int j=0;j<treeItems.count();j++)
            {
                if(treeItems.at(j)->data(id_idx)==treeItems.at(i)->data(parent_idx))
                {
                    treeItems.at(j)->appendChild(treeItems.at(i));
                    break;
                }
            }
        }
        else
        {
            m_rootItem->appendChild(treeItems.at(i));
        }
    }

    endResetModel();
    return true;


}

void SqlTreeModel::setColumnMapping(QMap<int, int> columnMap)
{
    m_columnMap=columnMap;
}

QVariant SqlTreeModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    if(role!=Qt::DisplayRole)
        return QVariant();

    SqlTreeItem * returnItem = static_cast<SqlTreeItem*>(index.internalPointer());
    if(m_columnMap.contains(index.column()))
    {
        return returnItem->data(m_columnMap[index.column()]);
    }
    return returnItem->data(index.column());
}

Qt::ItemFlags SqlTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    return QAbstractItemModel::flags(index);
}

QVariant SqlTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        if(m_columnMap.contains(section))
        {
            return m_rootItem->data(m_columnMap[section]);
        }
        return m_rootItem->data(section);
    }

    return QVariant();
}

bool SqlTreeModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    Q_UNUSED(role);

    if(orientation!=Qt::Horizontal)
        return false;

    m_rootItem->setData(section,value);

    emit headerDataChanged(orientation,0,m_rootItem->columnCount());

    return true;
}

QModelIndex SqlTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    SqlTreeItem *parentItem;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<SqlTreeItem*>(parent.internalPointer());

    SqlTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex SqlTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    SqlTreeItem *childItem = static_cast<SqlTreeItem*>(index.internalPointer());
    SqlTreeItem *parentItem = childItem->parentItem();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int SqlTreeModel::rowCount(const QModelIndex &parent) const
{
    SqlTreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_rootItem;
    else
        parentItem = static_cast<SqlTreeItem*>(parent.internalPointer());

//    qDebug()<<"row count: "<<parentItem->childCount()<<" (SqlTreeModel)";
    return parentItem->childCount();
}

int SqlTreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return static_cast<SqlTreeItem*>(parent.internalPointer())->columnCount();
//        qDebug()<<"Column count: "<<static_cast<SqlTreeItem*>(parent.internalPointer())->columnCount()<<" (SqlTreeModel)";
    }
    else
    {
//        qDebug()<<"Column count: "<<m_rootItem->columnCount()<<" (SqlTreeModel)";
        return m_rootItem->columnCount();
    }
}
