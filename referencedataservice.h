#ifndef REFERENCEDATASERVICE_H
#define REFERENCEDATASERVICE_H

#include <QList>
#include <QString>

struct ReferenceDataEntry {
  int id = -1;
  QString code;
  QString label;
};

struct ReferenceDataResult {
  bool success = false;
  QString message;
  int id = -1;
  static ReferenceDataResult ok(int id = -1) { return {true, {}, id}; }
  static ReferenceDataResult failure(const QString &message) {
    return {false, message, -1};
  }
};

class ReferenceDataService {
public:
  explicit ReferenceDataService(const QString &connectionName = {});
  void setConnectionName(const QString &connectionName);

  QList<ReferenceDataEntry> verificationMethods(QString *error = nullptr) const;
  QList<ReferenceDataEntry> requirementTypes(QString *error = nullptr) const;
  ReferenceDataResult addVerificationMethod(const QString &label) const;
  ReferenceDataResult updateVerificationMethod(int id, const QString &label) const;
  ReferenceDataResult deleteVerificationMethod(int id) const;
  ReferenceDataResult addRequirementType(const QString &code,
                                         const QString &label) const;
  ReferenceDataResult updateRequirementType(int id, const QString &code,
                                            const QString &label) const;
  ReferenceDataResult deleteRequirementType(int id) const;

private:
  ReferenceDataResult saveMethod(int id, const QString &label) const;
  ReferenceDataResult saveType(int id, const QString &code,
                               const QString &label) const;
  QString m_connectionName;
};

#endif
