#include "sqltreeitem.h"
#include <QDebug>

SqlTreeItem::SqlTreeItem(const QList<QVariant> &data, SqlTreeItem *parentItem)
{
    m_itemData=data;
    m_parentItem = parentItem;
}

SqlTreeItem::~SqlTreeItem()
{
    //destruction de tous les enfants de l'éléments
    qDeleteAll(m_childItems);
    //qDebug()<<"Destructeur appelé sur SqlTreeItem"<<this->data(1);
}

void SqlTreeItem::appendChild(SqlTreeItem *child)
{
    m_childItems.append(child);
    child->setParent(this);
}

SqlTreeItem *SqlTreeItem::child(int row)
{
    return m_childItems.value(row);
}

int SqlTreeItem::childCount() const
{
    return m_childItems.count();
}

int SqlTreeItem::columnCount() const
{
    return m_itemData.count();
}

QVariant SqlTreeItem::data(int column) const
{
    return m_itemData.value(column);
}

int SqlTreeItem::row() const
{
    if(m_parentItem)
    {
        return m_parentItem->m_childItems.indexOf(const_cast<SqlTreeItem*>(this));
    }

    return 0;
}

SqlTreeItem *SqlTreeItem::parentItem()
{
    return m_parentItem;
}

void SqlTreeItem::setParent(SqlTreeItem *parent)
{
    m_parentItem = parent;
}

void SqlTreeItem::removeChildren()
{
    for(int i=0;i<childCount();i++)
    {
        delete m_childItems.takeAt(i);
    }
}

void SqlTreeItem::setData(int col, const QVariant &data)
{
    if((col<0) || (col>m_itemData.count()-1))
        return;

    m_itemData.replace(col,data);
}
