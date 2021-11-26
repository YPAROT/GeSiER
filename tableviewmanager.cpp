#include "tableviewmanager.h"
#include <QSettings>
#include <QApplication>

TableViewManager::TableViewManager()
{

}



QString TableViewManager::requestForTableIndex(int tableIndex)
{
    if (tableIndex < m_customTablesList.count())
        return m_customTablesList.at(tableIndex).last();
    else
        return "";
}

QString TableViewManager::nameForTableIndex(int tableIndex)
{
    if (tableIndex < m_customTablesList.count())
        return m_customTablesList.at(tableIndex).first();
    else
        return "";
}

bool TableViewManager::setNameAndRequestForTableIndex(int tableIndex,QString newTableName,QString request)
{
    if (tableIndex < m_customTablesList.count())
   {
        m_customTablesList.removeAt(tableIndex);
        m_customTablesList.insert(tableIndex,QStringList() << newTableName << request);
        return true;
    }
    else
        return false;
}

bool TableViewManager::addNewTable(QString tableName,QString request)
{
    m_customTablesList.append(QStringList() << tableName << request);
    return true;
}


bool TableViewManager::loadConfFromFile(QString filename)
{
    if (QFile::exists(filename))
    {
        QSettings settings(filename,QSettings::IniFormat);

        m_customTablesList.clear();

        int count = settings.beginReadArray("tables");
        for (int index = 0; index < count;++index)
        {
            settings.setArrayIndex(index);
            m_customTablesList.append(
                        QStringList() << settings.value("name","Table "+QString::number(index)).toString()
                        << settings.value("request").toString());
        }
        settings.endArray();
        return true;
    }
    else
        return false;
}

void TableViewManager::saveConfToFile(QString filename)
{
    QSettings settings(filename,QSettings::IniFormat);

    settings.beginWriteArray("tables",m_customTablesList.count());
    for (int index = 0; index < m_customTablesList.count();++index)
    {
        settings.setArrayIndex(index);
        settings.setValue("name",m_customTablesList.at(index).first());
        settings.setValue("request",m_customTablesList.at(index).last());
    }
    settings.endArray();
}

QStringList TableViewManager::tablesNames()
{
    QStringList names;
    for (int index = 0; index< m_customTablesList.count();++index)
        names << m_customTablesList.at(index).first();

    return names;
}

bool TableViewManager::removeTableIndex(int tableIndex)
{
    if (tableIndex < m_customTablesList.count())
    {
        m_customTablesList.removeAt(tableIndex);
        return true;
    }
    else
        return false;
}
