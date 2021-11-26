//Ajouter un comparatif was/is afin de ne pas faire de setdata si le choix n'a pas changé.

#ifndef SQLPROXYRELATIONALDELEGATE_H
#define SQLPROXYRELATIONALDELEGATE_H

#include <QtSql/qtsqlglobal.h>
#include <QSortFilterProxyModel>

#ifdef QT_WIDGETS_LIB

#include <QtWidgets/qitemdelegate.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qcombobox.h>
#include <QtSql/qsqlrelationaltablemodel.h>

QT_BEGIN_NAMESPACE


class SqlProxyRelationalDelegate: public QItemDelegate
{
public:

    explicit SqlProxyRelationalDelegate(QObject *aParent = 0)
        : QItemDelegate(aParent)
    {}

    ~SqlProxyRelationalDelegate()
    {}

    QWidget *createEditor(QWidget *aParent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
    {

        //dépiler les proxy et index au fur à mesure jusqu'à obtenir un QSqlRelational table model et pas un proxy
        QModelIndex newIndex = index;
        const QSqlRelationalTableModel *sqlModel;
        const QSortFilterProxyModel *proxy = qobject_cast<const QSortFilterProxyModel *>(index.model());
        if(proxy)
        {
            newIndex=proxy->mapToSource(index);
            while(qobject_cast<QSortFilterProxyModel *>(proxy->sourceModel()))
            {
                proxy = qobject_cast<QSortFilterProxyModel *>(proxy->sourceModel());
                newIndex=proxy->mapToSource(newIndex);
            }

            sqlModel = qobject_cast<const QSqlRelationalTableModel *>(proxy->sourceModel());
        }
        else
        {
            sqlModel = qobject_cast<const QSqlRelationalTableModel *>(index.model());
        }


        QSqlTableModel *childModel = sqlModel ? sqlModel->relationModel(newIndex.column()) : 0;
        if (!childModel)
            return QItemDelegate::createEditor(aParent, option, index);

        QComboBox *combo = new QComboBox(aParent);
        combo->setModel(childModel);
        combo->setModelColumn(childModel->fieldIndex(sqlModel->relation(newIndex.column()).displayColumn()));
        combo->installEventFilter(const_cast<SqlProxyRelationalDelegate *>(this));

        return combo;

    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
    {
        if (!index.isValid())
            return;

        QModelIndex newIndex = index;
        QSqlRelationalTableModel *sqlModel;
        QSortFilterProxyModel *proxy = qobject_cast<QSortFilterProxyModel *>(model);
        if(proxy)
        {
            newIndex=proxy->mapToSource(index);
            while(qobject_cast<QSortFilterProxyModel *>(proxy->sourceModel()))
            {
                proxy = qobject_cast<QSortFilterProxyModel *>(proxy->sourceModel());
                newIndex=proxy->mapToSource(newIndex);
            }

            sqlModel = qobject_cast<QSqlRelationalTableModel *>(proxy->sourceModel());
        }
        else
        {
            sqlModel = qobject_cast<QSqlRelationalTableModel *>(model);
        }

        QSqlTableModel *childModel = sqlModel ? sqlModel->relationModel(newIndex.column()) : 0;
        QComboBox *combo = qobject_cast<QComboBox *>(editor);
        if (!sqlModel || !childModel || !combo) {
            QItemDelegate::setModelData(editor, model, index);
            return;
        }
        int currentItem = combo->currentIndex();

        //test si proxy sur model combobox (peut arriver car utilisé avec mapper)
        QSortFilterProxyModel * comboProxy = qobject_cast<QSortFilterProxyModel *>(combo->model());

        if(comboProxy)
        {
            qDebug()<<__FUNCTION__<<" Index du proxy: "<<comboProxy->index(currentItem,0);
            QModelIndex currentCombo = comboProxy->mapToSource(comboProxy->index(currentItem,0));
            currentItem=currentCombo.row();
        }

        int childColIndex = childModel->fieldIndex(sqlModel->relation(newIndex.column()).displayColumn());
        int childEditIndex = childModel->fieldIndex(sqlModel->relation(newIndex.column()).indexColumn());

        if((currentItem==-1) | (combo->currentText()=="NULL"))
        {
            bool setdisplay=sqlModel->setData(newIndex,QVariant(QString()),Qt::DisplayRole);//est considéré comme NULL par CustomModel
            bool setedit=sqlModel->setData(newIndex,QVariant(QString("-1")),Qt::EditRole); //est considéré comme NULL par CustomModel
            qDebug()<<"Set Data pour "<<childModel->tableName()<<" Display(\"\") set: "<<setdisplay<<" Edit (-1) set: "<<setedit<< " (SqlProxyRelationalDelegate)";
        }
        else
        {

            bool setdisplay=sqlModel->setData(newIndex,childModel->data(childModel->index(currentItem, childColIndex), Qt::DisplayRole),Qt::DisplayRole);

            bool setedit=sqlModel->setData(newIndex,childModel->data(childModel->index(currentItem, childEditIndex), Qt::EditRole),Qt::EditRole);

            qDebug()<<"Set Data pour "<<childModel->tableName()<<" Display("<<childModel->data(childModel->index(currentItem, childColIndex), Qt::DisplayRole).toString()<<") set: "<<setdisplay<<" Edit ("<<childModel->data(childModel->index(currentItem, childEditIndex), Qt::EditRole).toString()<<")set: "<<setedit<< " (SqlProxyRelationalDelegate)";
        }
    }


};

QT_END_NAMESPACE

#endif // QT_WIDGETS_LIB


#endif // SQLPROXYRELATIONALDELEGATE_H
