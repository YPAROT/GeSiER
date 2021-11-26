#ifndef SQLTREEITEM_H
#define SQLTREEITEM_H

#include <QVariant>
#include <QList>

class SqlTreeItem
{
public:
    explicit SqlTreeItem(const QList<QVariant> &data, SqlTreeItem *parentItem=0);
    ~SqlTreeItem();

    void appendChild(SqlTreeItem *child);
    SqlTreeItem *child(int row);
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    int row() const;
    SqlTreeItem *parentItem();
    void setParent(SqlTreeItem *parent);
    void removeChildren(void);
    void setData(int col, const QVariant &data);

private:
    QList<SqlTreeItem*> m_childItems;
    QList<QVariant> m_itemData;
    SqlTreeItem *m_parentItem;

};

#endif // SQLTREEITEM_H
