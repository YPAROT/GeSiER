#ifndef CHANGESERVICE_H
#define CHANGESERVICE_H

#include "requirementservice.h"
#include <QList>
#include <QString>

struct ChangeLink {
  QString objectType, label;
  int objectId = -1;
};
struct ChangeRecord {
  int id = -1, typeId = -1, statusId = -1;
  QString code, typeCode, typeLabel, statusCode, statusLabel, description,
      decision, openedAt, closedAt, externalReference, externalLink;
  bool finalStatus = false, archived = false;
  QList<ChangeLink> links;
};
struct ChangeFilter {
  QString text, objectType;
  int typeId = -1, statusId = -1, objectId = -1;
  bool includeArchived = false, onlyIncomplete = false;
};
struct ChangeCoverage {
  int total = 0, decided = 0, final = 0, complete = 0;
};

class ChangeService {
public:
  explicit ChangeService(QString connectionName = {});
  void setConnectionName(const QString &name);
  QList<ChangeRecord> find(const ChangeFilter &,
                           QString *error = nullptr) const;
  ChangeRecord get(int id, QString *error = nullptr) const;
  RequirementResult save(const ChangeRecord &);
  RequirementResult setArchived(int id, bool archived);
  RequirementResult saveType(int id, const QString &, const QString &,
                             bool active = true);
  RequirementResult saveStatus(int id, const QString &, const QString &,
                               bool isFinal, int position = 0);
  RequirementResult removeCatalogValue(const QString &table, int id);
  ChangeCoverage coverage(const ChangeFilter &filter = {},
                          QString *error = nullptr) const;
  RequirementResult exportXlsx(const QString &path,
                               const ChangeFilter &filter = {}) const;

private:
  QString m_connectionName;
};
#endif
