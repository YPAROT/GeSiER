#ifndef EDITREQUIREMENT_H
#define EDITREQUIREMENT_H

#include <QDialog>
#include <QSqlRelationalTableModel>
#include <QDataWidgetMapper>
#include <QSortFilterProxyModel>

namespace Ui {
class EditRequirement;
}

class EditRequirement : public QDialog
{
    Q_OBJECT

public:
    explicit EditRequirement(QAbstractItemModel *model,const QModelIndex &index,QWidget *parent = 0);
    ~EditRequirement();

    void ApplyFieldsChanges(void);

public slots:
    void OnDocIDChange(int index);

private:
    Ui::EditRequirement *ui;
    QDataWidgetMapper * m_mapper;
    QSortFilterProxyModel *m_chapterProxy;
    QSortFilterProxyModel *m_reqDocProxy;
};

#endif // EDITREQUIREMENT_H
