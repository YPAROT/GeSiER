#ifndef DATABASEMIGRATOR_H
#define DATABASEMIGRATOR_H

#include <QSqlDatabase>
#include <QString>

class DatabaseMigrator {
public:
  static const int CurrentVersion = 16;

  static bool migrate(QSqlDatabase db, QString *errorMessage = nullptr);

private:
  static bool execute(QSqlDatabase db, const QString &sql,
                      QString *errorMessage);
  static bool addColumnIfMissing(QSqlDatabase db, const QString &table,
                                 const QString &column,
                                 const QString &definition,
                                 QString *errorMessage);
};

#endif // DATABASEMIGRATOR_H
