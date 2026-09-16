#ifndef HISTORYSERVICE_H
#define HISTORYSERVICE_H

#include <QList>
#include <QString>

struct HistoryRecord {
  int id = -1;
  QString time, author, eventType, objectType, beforeJson, afterJson, comment,
      groupId;
  int objectId = -1;
};

struct ProjectDiagnostic {
  bool ok = false;
  int schemaVersion = 0;
  qint64 fileSize = 0;
  QStringList errors;
  QStringList warnings;
  QString summary() const;
};

class HistoryService {
public:
  explicit HistoryService(QString connectionName);
  QList<HistoryRecord> find(const QString &text = {},
                            const QString &objectType = {},
                            const QString &eventType = {},
                            int objectId = -1) const;
  QStringList objectTypes() const;
  QStringList eventTypes() const;
  ProjectDiagnostic diagnose() const;
  bool setAuthor(const QString &author, QString *error = nullptr) const;

private:
  QString m_connectionName;
};

#endif
