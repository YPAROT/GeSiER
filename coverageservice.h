#ifndef COVERAGESERVICE_H
#define COVERAGESERVICE_H

#include <QList>
#include <QString>

struct CoverageScope {
  QList<int> statusIds;
  int ptId = -1;
  int documentId = -1;
  int configurationId = -1;
  int typeId = -1;
  QString search;
  bool includeObsolete = false;
};

struct CoverageSlice {
  int id = -1;
  QString code;
  QString label;
  int value = 0;
};

struct ConfigurationCoverage {
  int id = -1;
  QString code;
  QString label;
  int applicable = 0;
  int total = 0;
  bool active = true;
  int percent() const;
};

struct CoverageMetric {
  QString key;
  QString label;
  int covered = 0;
  int total = 0;
  int page = 1;
  QString coveredFilter;
  QString missingFilter;

  int percent() const;
};

struct CoverageSnapshot {
  QList<CoverageMetric> metrics;
  QList<CoverageSlice> requirementStatuses;
  QList<CoverageSlice> changeTypes;
  QList<CoverageSlice> changeStatuses;
  QList<CoverageSlice> documentTypes;
  QList<ConfigurationCoverage> configurations;
  int requirements = 0;
  int documents = 0;
  int changes = 0;
  int openChanges = 0;
  int activeConfigurations = 0;
  int recentPublications = 0;
  int neverPublishedDocuments = 0;
};

class CoverageService {
public:
  explicit CoverageService(QString connectionName = {});
  void setConnectionName(const QString &connectionName);
  bool isAvailable() const;
  QList<CoverageMetric> metrics(const CoverageScope &scope = {},
                                QString *error = nullptr) const;
  CoverageSnapshot snapshot(const CoverageScope &scope = {},
                            QString *error = nullptr) const;

private:
  QString m_connectionName;
};

#endif
