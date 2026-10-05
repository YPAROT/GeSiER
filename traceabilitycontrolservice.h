#ifndef TRACEABILITYCONTROLSERVICE_H
#define TRACEABILITYCONTROLSERVICE_H

#include <QList>
#include <QString>
#include <QStringList>

struct TraceabilityFilter {
  QString text;
  int statusId = -1;
  int applicable = -1;
  int takenIntoAccount = -1;
  int rootConform = -1;
  bool includeObsolete = false;
};

struct TraceabilityControlRow {
  int requirementId = -1;
  int statusId = -1;
  int primaryPtId = -1;
  QString code;
  QString title;
  QString status;
  QString primaryPt;
  bool obsolete = false;
  bool applicable = false;
  bool rootConform = false;
  int qualifyingRelationCount = 0;
  QStringList relationTypes;
  bool takenIntoAccount = false;
  QString diagnostic;
};

class TraceabilityControlService {
public:
  explicit TraceabilityControlService(QString connectionName = {});
  void setConnectionName(const QString &connectionName);
  QList<TraceabilityControlRow> rows(const TraceabilityFilter &filter = {},
                                     QString *error = nullptr) const;
  int activeRootId(QString *error = nullptr) const;
  int activeConfigurationCount(QString *error = nullptr) const;

private:
  QString m_connectionName;
};

#endif
