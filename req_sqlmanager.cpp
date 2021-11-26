#include "req_sqlmanager.h"

REQ_SQLManager::REQ_SQLManager()
{
    m_DBConnectionName = "REQ_DB";
}

QSqlError REQ_SQLManager::newDB(QString filename)
{
    QSqlError err;

    if (!filename.isEmpty())
    {

        if (QSqlDatabase::contains(m_DBConnectionName))
            close();

        err = openDB(filename);
        if (err.type() == QSqlError::NoError)
        {
            QFile file(":/files/createTables.sql");
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            QTextStream in(&file);
            QString fileContent = in.readAll();
            file.close();
            QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName);
            if(!db.isValid())
                return err;
            QSqlQuery query("", db);
            QStringList queries = fileContent.split("\n",QString::SkipEmptyParts);
            for (int i=0; i< queries.count() ; ++i)
            {
                if (!query.exec(queries.at(i)))
                {
                    m_lastError = query.lastError().text();
                    qWarning() << "ERREUR: " << query.lastError().text() << query.lastQuery();
                    query.finish();
                }
            }
        }
        else
        {
            m_lastError = err.text();
            qWarning() << "ERREUR: " << m_lastError;
        }
    }
    return err;
}

QSqlError REQ_SQLManager::openDB(QString filename)
{
    QSqlError err;
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_DBConnectionName);

    if(!db.isValid())
        return err; //voir si on peut ne pas renvoyer une erreur sql nulle ...

    db.setDatabaseName(filename);

    if (!db.open())
    {
        err = db.lastError();
//        QSqlDatabase::removeDatabase(m_DBConnectionName);
        close();
    }
    else
    {
        m_filename = filename;
        QFileInfo fileInfo(filename);

        QString backupDirectory = QApplication::applicationDirPath()+"/dbBackups/";
        QDir dir;
        dir.mkpath(backupDirectory);
        QString backupFilename = backupDirectory + fileInfo.completeBaseName()+QDateTime::currentDateTime().toString("_yyyyMMdd_hhmmss.")+fileInfo.suffix();
        QFile::copy(filename,backupFilename);
    }

    //activer la gestion des clefs étrangères
    db.exec("PRAGMA foreign_keys = ON");

    return err;
}

void REQ_SQLManager::close()
{
    //Check existence connection
    if (!QSqlDatabase::contains(m_DBConnectionName))
        return;
    //fermeture DB
    {
        QSqlDatabase db=QSqlDatabase::database(m_DBConnectionName);
        db.commit();
        db.close();
    }
    //Fermeture connection
    QSqlDatabase::removeDatabase(m_DBConnectionName);
}

bool REQ_SQLManager::saveAs(QString filename)
{
    if (m_filename.isEmpty())
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName);

    if(!db.isValid())
        return false;

//    if (db.isOpen())
//    {
        close();
        if (QFile::exists(filename))
        {
            if (QFile::remove(filename) == false)
            {
                openDB(m_filename);
                return false;
            }
        }

        if (QFile::copy(m_filename,filename))
        {
            QFile::remove(m_filename);
            openDB(filename);
            return db.isOpen();
        }
        else
        {
            openDB(m_filename);
            return false;
        }
//    }
    return false;
}

QString REQ_SQLManager::currentConnection() const
{
    return m_DBConnectionName;
}

bool REQ_SQLManager::execQuery(QString queryStr)
{
    QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName);

    if(!db.isValid())
        return false;

//    if (db.isOpen())
//    {
        QSqlQuery query("", db);

        if(!query.exec(queryStr))
        {

            m_lastError = query.lastError().text();
            qWarning() << "ERREUR: " << query.lastError().text() << query.lastQuery();
            query.finish();
            return false;
        }
        else
        {
            query.finish();
            return true;
        }
//    }
    return false;
}

QVector<QStringList> REQ_SQLManager::execQueryAndGetResults(QString queryStr)
{
    QStringList list;
    QVector<QStringList> result;

    QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName);

    if(!db.isValid())
        return QVector<QStringList>();

//    if (db.isOpen())
//    {
        QSqlQuery query("", db);
        if(!query.exec(queryStr))
        {
            m_lastError = query.lastError().text();
            qWarning() << "ERREUR: " << query.lastError().text() << query.lastQuery();
            query.finish();
        }
        else if(query.first())
        {
            list.clear();
            if (query.record().count() > 0)
            {
                list<<query.value(0).toString();
                for (int i=1; i < query.record().count();++i)
                    list<<query.value(i).toString();
                result.append(list);
            }
            while(query.next())
            {   list.clear();
                if (query.record().count() > 0)
                {
                    list<<query.value(0).toString();
                    for (int i=1; i < query.record().count();++i)
                        list<<query.value(i).toString();
                    result.append(list);
                }
            }
            query.finish();
        }
//    }
    return result;
}

QStringList REQ_SQLManager::getColumnFromQuery(QString queryStr, int colnum)
{
    QStringList list;

    QSqlDatabase db = QSqlDatabase::database(m_DBConnectionName);

    if(!db.isValid())
        return QStringList();

//    if (db.isOpen())
//    {
        QSqlQuery query("", db);
        if(!query.exec(queryStr))
        {
            m_lastError = query.lastError().text();
            qWarning() << "ERREUR: " << query.lastError().text() << query.lastQuery();
            query.finish();
        }
        else if(query.first())
        {
            if(query.record().count()>colnum)
            {
                list << query.value(colnum).toString();
                while(query.next())
                {
                    list << query.value(colnum).toString();
                }
                query.finish();
            }
        }
//    }
    return list;
}

