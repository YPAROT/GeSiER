#include <QtTest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "databasemigrator.h"

class DatabaseMigratorTest : public QObject
{
    Q_OBJECT
private slots:
    void migratesLegacySchemaWithoutLosingLinks();
};

void DatabaseMigratorTest::migratesLegacySchemaWithoutLosingLinks()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE","migration-test");
    db.setDatabaseName(directory.filePath("legacy.db"));
    QVERIFY2(db.open(),qPrintable(db.lastError().text()));
    QSqlQuery query(db);
    const QStringList legacy={
        "CREATE TABLE PT(ID INTEGER PRIMARY KEY,NAME TEXT,PARENT INTEGER)",
        "CREATE TABLE REQ_STATUS(ID INTEGER PRIMARY KEY,SHORTCUT TEXT,STATUS TEXT)",
        "CREATE TABLE REQ_METHOD(ID INTEGER PRIMARY KEY,METHOD TEXT)",
        "CREATE TABLE REQUIREMENT(ID INTEGER PRIMARY KEY,PT_ID INTEGER,CODE TEXT,DOC_ID INTEGER,DOC_CHAPTER INTEGER,TITLE TEXT,DESCRIPTION TEXT,TYPE INTEGER,STATUS INTEGER,SOURCE TEXT,PARENT_ID INTEGER,VERIF_LEVEL TEXT,VERIF_METHOD INTEGER,COMMENTS TEXT)",
        "CREATE TABLE DOCUMENT(ID INTEGER PRIMARY KEY,PT_ID INTEGER,TYPE INTEGER,TITLE TEXT,DESCRIPTION TEXT)",
        "CREATE TABLE INTERFACE(ID INTEGER PRIMARY KEY,ELEMENT1 INTEGER,ELEMENT2 INTEGER,DOC_ID INTEGER,DESCRIPTION TEXT,DOC_CHAPTER INTEGER)",
        "INSERT INTO PT VALUES(1,'SYS',NULL)",
        "INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,PARENT_ID,VERIF_METHOD) VALUES(1,1,'SYS-001',1,'Parent',NULL,1)",
        "INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,PARENT_ID,VERIF_METHOD) VALUES(2,1,'SYS-002',1,'Enfant',1,1)"
    };
    for(const QString &sql:legacy) QVERIFY2(query.exec(sql),qPrintable(query.lastError().text()));
    QString error;
    QVERIFY2(DatabaseMigrator::migrate(db,&error),qPrintable(error));
    QVERIFY(query.exec("PRAGMA user_version")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(),DatabaseMigrator::CurrentVersion);
    QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_RELATION WHERE SOURCE_REQ_ID=1 AND TARGET_REQ_ID=2 AND TYPE_ID=1")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(),1);
    QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_PT WHERE REQ_ID IN (1,2)")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(),2);
    QVERIFY(!query.exec("INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID) VALUES(2,1,1)"));
    QVERIFY(query.exec("SELECT COUNT(*) FROM EVENT_LOG WHERE OBJECT_TYPE='REQUIREMENT_RELATION' AND EVENT_TYPE='CREATE'")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(),1);
    QVERIFY2(DatabaseMigrator::migrate(db,&error),qPrintable(error));
    QVERIFY(query.exec("SELECT COUNT(*) FROM REQUIREMENT_RELATION WHERE SOURCE_REQ_ID=1 AND TARGET_REQ_ID=2 AND TYPE_ID=1")); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(),1);
    db.close();
}

QTEST_GUILESS_MAIN(DatabaseMigratorTest)
#include "database_migrator_test.moc"
