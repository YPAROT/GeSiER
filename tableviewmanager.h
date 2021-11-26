#ifndef TABLEVIEWMANAGER_H
#define TABLEVIEWMANAGER_H

#include <QtCore>

class TableViewManager
{
public:
    TableViewManager();

    QString requestForTableIndex(int tableIndex);
    QString nameForTableIndex(int tableIndex);

    bool setNameAndRequestForTableIndex(int tableIndex,QString newTableName,QString request);
    bool addNewTable(QString tableName,QString request);

    bool removeTableIndex(int tableIndex);
    bool loadConfFromFile(QString filename);
    void saveConfToFile(QString filename);
    QStringList tablesNames();

private:
    QList<QStringList> m_customTablesList;
};

#endif // TABLEVIEWMANAGER_H
