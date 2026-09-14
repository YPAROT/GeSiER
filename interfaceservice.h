#ifndef INTERFACESERVICE_H
#define INTERFACESERVICE_H

#include "requirementservice.h"
#include <QList>
#include <QMap>
#include <QString>

struct InterfaceRecord {
  int id = -1, element1Id = -1, element2Id = -1;
  QString code, description, status, element1, element2, types, requirements,
      documents;
  QList<int> typeIds, requirementIds, documentIds;
  QMap<int, int> documentChapters;
  bool archived = false;
};

struct InterfaceFilter {
  QString text;
  int ptId = -1, typeId = -1;
  bool includeArchived = false, onlyWithoutIcd = false;
};

struct N2Cell {
  int firstPtId = -1, secondPtId = -1, interfaceCount = 0, coveredCount = 0,
      typeCount = 0;
  QString label;
};

class InterfaceService {
public:
  explicit InterfaceService(QString connectionName = {});
  void setConnectionName(const QString &name);
  QList<InterfaceRecord> find(const InterfaceFilter &filter,
                              QString *error = nullptr) const;
  InterfaceRecord get(int id, QString *error = nullptr) const;
  RequirementResult save(const InterfaceRecord &record);
  RequirementResult setArchived(int id, bool archived);
  RequirementResult saveType(int id, const QString &code, const QString &label);
  RequirementResult removeType(int id);
  QList<N2Cell> matrix(bool includeArchived = false,
                       QString *error = nullptr) const;
  RequirementResult exportXlsx(const QString &path,
                               const InterfaceFilter &filter) const;

private:
  QString m_connectionName;
};

#endif
