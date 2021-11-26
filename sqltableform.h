#ifndef SQLTABLEFORM_H
#define SQLTABLEFORM_H

#include <QWidget>
#include <QLineEdit>
#include <QTimer>
#include <QSqlRelationalTableModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QList>
#include <QStringList>
#include <QPair>
#include <QAbstractItemView>
#include "editrequirement.h"
#include <QDebug>


namespace Ui {
class SQLTableForm;
}

class SQLTableForm : public QWidget
{
    Q_OBJECT

public:
    explicit SQLTableForm(QWidget *parent = nullptr);
    ~SQLTableForm();

    void setEditable(bool editable);
    void execQuery(QString queryStr, QString dbConnectionName);
    void showTable(const QString &t, QString dbConnectionName);
    QStringList selectedRowsString(int columnIndex);
    QTableView* sqlTableView();
    void setColumnHidden(int column, bool hide);
    void refreshTable(bool newTable = false);
    QString tableName() {return m_tableName;}
    void addRelationalTable(int colonne, QSqlRelation relation);
    void removeRelationalTable(int index);
    void removeRelationalTable(QString tableName);
    QSqlRelation getRelationAtColumn(int index);
    void SetEditionTriggers(QAbstractItemView::EditTriggers triggers);
    void SetHorizontalHeaders(int section, const QVariant &value, int role = Qt::EditRole);
    QStringList getHorizontalHeadersNames();
    int getFieldIndex(const QString fieldName);
    QStringList getFieldNames();
    void insertRow(QMap<int, QVariant> fieldsValue);
    void updateRow(int row,QMap<int, QVariant> fieldsValue);
    int findRow(int field, QVariant uniqueParameterValue);
    QSqlIndex primaryKey();

protected slots:
    void resultSectionResized(int logicalIndex, int oldSize, int newSize);

    void lineEditorChanged();
    void lineEditorJustChanged();
    void insertRow();
    void deleteRow();
    void resizeTable();
    void comitAll();
    void revertAll();
    void sortIndicatorChanged(int column, Qt::SortOrder order);

signals:
    void statusMessage(const QString &message);
    void itemDoubleClicked(const QModelIndex &index);
    void comitOccured(void);
private slots:
    void on_sqlTableView_doubleClicked(const QModelIndex &index);

private:
    Ui::SQLTableForm *ui;

    void updateHeaders();

    QSqlRelationalTableModel *m_sqlTableModel;
    QAction *m_insertRowAction;
    QAction *m_deleteRowAction;
    QAction *m_commitAction;
    QAction *m_revertAction;
    QAction *m_resizeTableAction;
    bool m_isEditable;
    QVector<QLineEdit*> m_lineEditors;
    QTimer *m_lineEditorChangeTimer;
    QVector<QSortFilterProxyModel *> m_proxies;

//    QSqlDatabase m_db;
    QString m_dbConnectionName;
    QString m_tableName;
    QList<QPair<int,QSqlRelation>> m_relationList;

    QMap<int,QPair<QVariant,int>> m_headerMap;

};


class CustomModel: public QSqlRelationalTableModel
{
    Q_OBJECT
public:
    explicit CustomModel(QObject *parent = nullptr, QSqlDatabase db = QSqlDatabase())
        : QSqlRelationalTableModel(parent, db) {setJoinMode(QSqlRelationalTableModel::LeftJoin);}

    QVariant data(const QModelIndex &idx, int role) const override
    {
        if (role == Qt::BackgroundRole && isDirty(idx))
            return QBrush(QColor(Qt::yellow));

        if(role==Qt::FontRole && QSqlRelationalTableModel::data(idx,Qt::DisplayRole).isNull())
        {
            QFont nullFont;
            nullFont.setItalic(true);
            return nullFont;
        }

        if(role==Qt::ForegroundRole && QSqlRelationalTableModel::data(idx,Qt::DisplayRole).isNull())
        {
            QBrush textbrush;
            textbrush.setColor(Qt::gray);
            return textbrush;
        }

        if(role==Qt::DisplayRole && QSqlRelationalTableModel::data(idx,Qt::DisplayRole).isNull())
        {
            return QString("NULL");
        }

        return QSqlRelationalTableModel::data(idx, role);
    }

    //réécrire le setdata pour pouvoir prendre NULL en input
    //vérifier que l'ancienne data n'est pas la même que la nouvelle (cas des combo box par exemple)
    //pour éviter d'éditer et de mettre dirty.
    bool setData(const QModelIndex& idx, const QVariant& value, int role) override
    {
        QVariant mod_value=value;

        if(!idx.isValid())
        {
            qDebug()<<"Index non valide"<<" (CustomModel)";
            return false;
        }

        //On recherche la valeur que prends value dans le cas ou il y a une relation sur la colonne
        QSqlRelation rel=relation(idx.column());
        if(rel.isValid() & (role==Qt::EditRole) & value.isValid())
        {
            //get child
            QSqlTableModel* relModel= relationModel(idx.column());

            if(!relModel)
            {
                qDebug()<<"Pas de modele relationel"<<" (CustomModel)";
                return false;
            }

            int col=relModel->fieldIndex(rel.displayColumn());
            QModelIndexList primKeyFinds=relModel->match(relModel->index(relModel->fieldIndex(rel.indexColumn()),0),Qt::DisplayRole,value,-1);
            if(primKeyFinds.count()<1)
            {
                qDebug()<<"Clef primaire non trouvée"<<" (CustomModel)";
                //On veut rentrer NULL comme valeur.
                return QSqlTableModel::setData(idx,QVariant(QString()),role);
            }

            int row=primKeyFinds.at(0).row();
            mod_value = relModel->data(relModel->index(row,col));

            qDebug()<<"valeur editRole modifiée: WAS: "<<value<<" IS: "<<mod_value<<" (CustomModel)";
        }

        //si la valeur renvoyée est la même que la valeur dans le model on sort
        if((mod_value==data(idx,role)) & mod_value.isValid())
        {
            return true;
        }


        //test si value est vide: si oui on met à NULL la valeur dans la DB
        if((role==Qt::DisplayRole || role==Qt::EditRole) && (value.toString()==""||value.toString()=="NULL"))
        {
            qDebug()<<"Le variant envoyé comme NULL est: "<<QVariant(QString())<<" (CustomModel)";
            return QSqlTableModel::setData(idx,QVariant(QString()),role); //il ne faut pas être géné par des relations si on veut mettre NULL
        }

        return QSqlRelationalTableModel::setData(idx,value,role);
    }
};
#endif // SQLTABLEFORM_H
