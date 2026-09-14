#ifndef APPLICABILITYSERVICE_H
#define APPLICABILITYSERVICE_H

#include "requirementservice.h"
#include <QList>
#include <QString>

struct ConfigurationRecord {
  int id = -1;
  QString code, label, description, createdAt, updatedAt;
  int position = 0, useCount = 0;
  bool active = true;
};

struct ApplicabilityFilter {
  int ptRootId = -1, documentId = -1, statusId = -1, typeId = -1;
  bool includeObsoleteRequirements = false;
  bool includeArchivedConfigurations = false;
};

struct ApplicabilityCell {
  int requirementId = -1, configurationId = -1, sourcePtId = -1;
  bool applicable = false, explicitValue = false;
  QString comment;
};

struct ApplicabilityRow {
  int requirementId = -1;
  QString code, title, status, type, productTrees, document;
  QList<ApplicabilityCell> cells;
};

class ApplicabilityService {
public:
  explicit ApplicabilityService(QString connectionName = {});
  void setConnectionName(const QString &name);
  QList<ConfigurationRecord> configurations(bool includeArchived = true,
                                             QString *error = nullptr) const;
  ConfigurationRecord configuration(int id) const;
  RequirementResult saveConfiguration(const ConfigurationRecord &record);
  RequirementResult setConfigurationActive(int id, bool active);
  QList<ApplicabilityRow> matrix(const ApplicabilityFilter &filter,
                                 QString *error = nullptr) const;
  RequirementResult setApplicability(int requirementId, int configurationId,
                                     int ptId, bool applicable,
                                     const QString &comment = {});
  RequirementResult clearOverride(int requirementId, int configurationId,
                                  int ptId);
  double rate(const QList<ApplicabilityRow> &rows) const;
  RequirementResult exportXlsx(const QString &path,
                               const ApplicabilityFilter &filter) const;

private:
  QString m_connectionName;
};

#endif
