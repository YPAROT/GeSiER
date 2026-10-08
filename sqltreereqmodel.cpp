#include "sqltreereqmodel.h"


SqlTreeReqModel::SqlTreeReqModel(const QSqlDatabase &db, QObject *parent):QAbstractItemModel(parent)
{
    m_db = db;
    m_columnMap.clear();
    m_rootItem = Q_NULLPTR;
}

SqlTreeReqModel::~SqlTreeReqModel()
{
    delete m_rootItem;
}

bool SqlTreeReqModel::setRelation(QSqlRelation relation) //obsolète ici car dépendant de la table
{
    m_relation = relation;
    if (!relation.isValid())
        return false;
    return true;
}

bool SqlTreeReqModel::select(QVariant reqID)
{

    //Il faut effacer le modèle si il y a déjà des données (on ne garde que le root).
    beginResetModel();
    if(m_rootItem)
    {
        delete m_rootItem;
    }

    // On exécute un select sur le Requirement en question pour obtenir les descendants/ascendants

    QSqlQuery queryChild("WITH RECURSIVE Child AS ("
                    "SELECT ID,CODE,TITLE,NULL AS PARENT_ID,0 AS LEVEL"
                    " FROM REQUIREMENT R WHERE R.ID="+reqID.toString()+
                    " UNION SELECT R.ID,R.CODE,R.TITLE,L.SOURCE_REQ_ID AS PARENT_ID,Child.LEVEL+1"
                    " FROM REQUIREMENT R JOIN REQUIREMENT_RELATION L ON L.TARGET_REQ_ID=R.ID"
                    " JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=L.TYPE_ID AND T.CODE='DECOMPOSE'"
                    " JOIN Child ON L.SOURCE_REQ_ID=Child.ID"
                    " ORDER BY 5 DESC)"
                    " SELECT * FROM Child",m_db);
//    if(!queryChild.first())
//        return false;

    QSqlQuery queryParents("WITH RECURSIVE parents AS ("
                           "SELECT R.ID,R.CODE,R.TITLE,(SELECT L.SOURCE_REQ_ID FROM REQUIREMENT_RELATION L JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=L.TYPE_ID WHERE L.TARGET_REQ_ID=R.ID AND T.CODE='DECOMPOSE' ORDER BY L.ID LIMIT 1) AS PARENT_ID,0 AS LEVEL "
                           "FROM REQUIREMENT R WHERE R.ID="+reqID.toString()+" UNION ALL "
                           "SELECT R.ID,R.CODE,R.TITLE,(SELECT L2.SOURCE_REQ_ID FROM REQUIREMENT_RELATION L2 JOIN REQUIREMENT_RELATION_TYPE T2 ON T2.ID=L2.TYPE_ID WHERE L2.TARGET_REQ_ID=R.ID AND T2.CODE='DECOMPOSE' ORDER BY L2.ID LIMIT 1),parents.LEVEL-1 "
                           "FROM REQUIREMENT R JOIN parents ON R.ID=parents.PARENT_ID) "
                           "SELECT * FROM parents "
                           "ORDER BY 5 ASC",m_db);


//    if(!queryParents.first())
//        return false;

    //on ajoute les headers
    QList<QVariant> headers;
    headers<<QVariant("ID")<<QVariant("Code")<<QVariant("Titre")<<QVariant("Parent")<<QVariant("Niveau");

    m_rootItem = new SqlTreeItem(headers);

    //L'élément de plus haut niveau est le premier des parents, c'est lui qu'il faut créer en premier pour y attacher les autres.
    SqlTreeItem * newEntry,*lastParent;
    {
        queryParents.first();
        QSqlRecord rec=queryParents.record();
        QList<QVariant> data;
        for(int i=0;i<rec.count();i++)
        {
            data.append(rec.value(i));
        }
        newEntry = new SqlTreeItem(data,m_rootItem);
        m_rootItem->appendChild(newEntry);
        lastParent=newEntry;
    }
    //les autres suivent par ordre de "profondeur"
    while (queryParents.next())
    {
        QSqlRecord rec=queryParents.record();
        QList<QVariant> data;
        for(int i=0;i<rec.count();i++)
        {
            data.append(rec.value(i));
        }
        newEntry = new SqlTreeItem(data,lastParent);
        lastParent->appendChild(newEntry);
        lastParent=newEntry;// dans le cas ou on remonte les parents il n'y a pas de "branche" possible
    }

    //Maintenant on descend vers les enfants

    //le premier record est le requirement dont on trace l'arbre. Il est déjà renseigné
    queryChild.first();
    queryChild.record();
    QList<SqlTreeItem*> parentList;
    parentList.append(lastParent);
    while (queryChild.next())
    {
        QSqlRecord rec=queryChild.record();
        QList<QVariant> data;
        for(int i=0;i<rec.count();i++)
        {
            data.append(rec.value(i));
        }

        if(data.value(4).toInt()>parentList.last()->data(4).toInt())
        {
            parentList.last()->appendChild(new SqlTreeItem(data,parentList.last()));
            parentList<<parentList.last()->child(parentList.last()->childCount()-1);
        }
        else if (data.value(4).toInt()==parentList.last()->data(4).toInt())
        {
            parentList.pop_back();
            parentList.last()->appendChild(new SqlTreeItem(data,parentList.last()));
            parentList<<parentList.last()->child(parentList.last()->childCount()-1);
        }
        else
        {
            while((data.value(4).toInt()<=parentList.last()->data(4).toInt()) && (parentList.count()>0))
            {
                parentList.pop_back();
            }
            parentList.last()->appendChild(new SqlTreeItem(data,parentList.last()));
            parentList<<parentList.last()->child(parentList.last()->childCount()-1);
        }


    }

    endResetModel();

    return true;


}

void SqlTreeReqModel::setColumnMapping(QMap<int, int> columnMap)
{
    m_columnMap=columnMap;
}

QVariant SqlTreeReqModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    if((role!=Qt::DisplayRole) && (role!=Qt::FontRole))
        return QVariant();

    SqlTreeItem * returnItem = static_cast<SqlTreeItem*>(index.internalPointer());

    if(role==Qt::FontRole)
    {
        if(returnItem->data(4).toInt()==0)
        {
            QFont parentFont;
            parentFont.setBold(true);
            return parentFont;
        }
    }

    if(m_columnMap.contains(index.column()))
    {
        return returnItem->data(m_columnMap[index.column()]);
    }
    return returnItem->data(index.column());
}

Qt::ItemFlags SqlTreeReqModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return QAbstractItemModel::flags(index);
}

QVariant SqlTreeReqModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QModelIndex SqlTreeReqModel::index(int row, int column, const QModelIndex &parent) const
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

QModelIndex SqlTreeReqModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    SqlTreeItem *childItem = static_cast<SqlTreeItem*>(index.internalPointer());
    SqlTreeItem *parentItem = childItem->parentItem();

    if (parentItem == m_rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int SqlTreeReqModel::rowCount(const QModelIndex &parent) const
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

int SqlTreeReqModel::columnCount(const QModelIndex &parent) const
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
