#ifndef REQ_SQLMANAGER_H
#define REQ_SQLMANAGER_H

#include <QSqlQuery>
#include <QStringList>
#include <QVector>
#include <QSqlRecord>
#include <QSqlError>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDateTime>

class REQ_SQLManager
{
public:
    REQ_SQLManager();


    QSqlError newDB(QString filename);
    QSqlError openDB(QString filename);
    void close();
    bool saveAs(QString filename);
    QString currentConnection() const;
    bool execQuery(QString queryStr);
    QVector<QStringList> execQueryAndGetResults(QString queryStr);
    QStringList getColumnFromQuery(QString queryStr, int colnum=0);
    QString lastError()const { return m_lastError;}

private:
    QString m_filename;
//    QSqlDatabase m_db;
    QString m_DBConnectionName;
    QString m_lastError;
};

#endif // REQ_SQLMANAGER_H
