#ifndef REQUIREMENTRELATIONSERVICE_H
#define REQUIREMENTRELATIONSERVICE_H

#include "requirementservice.h"

struct RequirementRelationRecord {
  int id = -1;
  int sourceId = -1;
  int targetId = -1;
  int typeId = -1;
  QString typeCode;
  QString otherCode;
  QString otherTitle;
  QString otherStatus;
  QString otherType;
  QString otherProductTrees;
  QString comment;
  bool outgoing = false;
};

class RequirementRelationService {
public:
  explicit RequirementRelationService(QString connectionName = {});
  QList<RequirementRelationRecord> relations(int requirementId,
                                             QString *error = nullptr) const;
  RequirementResult add(int sourceId, int targetId, int typeId,
                        const QString &comment = {});
  RequirementResult remove(int relationId);
  RequirementResult updateComment(int relationId, const QString &comment);

private:
  QString m_connectionName;
};

#endif
