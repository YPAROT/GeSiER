#ifndef AUDITSERVICE_H
#define AUDITSERVICE_H

#include <QJsonObject>
#include <QList>
#include <QString>

class QSqlDatabase;

struct AuditSubject {
  QString objectType;
  int objectId = -1;
  QString role = "AFFECTED";
};

class AuditService {
public:
  static QJsonObject requirementSnapshot(QSqlDatabase db, int requirementId);
  static QList<AuditSubject> requirementSubjects(QSqlDatabase db,
                                                  int requirementId);
  static bool record(QSqlDatabase db, const QString &eventType,
                     const QString &objectType, int objectId,
                     const QJsonObject &before, const QJsonObject &after,
                     const QList<AuditSubject> &subjects = {},
                     const QString &comment = {}, QString *error = nullptr);
};

#endif
