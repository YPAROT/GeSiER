#ifndef REQUIREMENTSERVICE_H
#define REQUIREMENTSERVICE_H
#include <QList>
#include <QString>

struct RequirementFilter {
  QString code;
  QString title;
  QString description;
  QString source;
  QList<int> statusIds;
  QList<int> typeIds;
  QList<int> ptIds;
  QList<int> methodIds;
  QList<int> configurationIds;
  bool includeObsolete = false;
  int allocated = -1;
  int traced = -1;
  int documented = -1;
  int verified = -1;
};
struct RequirementVerification {
  int id = -1;
  int methodId = -1;
  int levelPtId = -1;
  QString level;
  QString procedure;
  QString redmine;
  QString means;
  QString verdict;
  QString comment;
  int position = 0;
};
struct RequirementRecord {
  int id = -1;
  QString code, title, description, source;
  QString productTrees, verificationMethods, applicability;
  bool allocated = false, traced = false, documented = false, verified = false;
  bool traceRoot = false;
  int typeId = -1, statusId = -1, primaryPtId = -1;
  QList<int> ptIds;
  QList<int> configurationIds;
  QList<RequirementVerification> verifications;
};
struct RequirementResult {
  bool success = false;
  QString message;
  int id = -1;
  QString code;

  static RequirementResult failure(const QString &message) {
    RequirementResult result;
    result.message = message;
    return result;
  }
  static RequirementResult successResult(const QString &message, int id,
                                         const QString &code = QString()) {
    RequirementResult result;
    result.success = true;
    result.message = message;
    result.id = id;
    result.code = code;
    return result;
  }
};

class RequirementService {
public:
  explicit RequirementService(QString connectionName = {});
  void setConnectionName(const QString &);
  QList<RequirementRecord> find(const RequirementFilter &,
                                QString *error = nullptr) const;
  RequirementRecord get(int id, QString *error = nullptr) const;
  QString suggestCode(int ptId) const;
  RequirementResult save(const RequirementRecord &, bool manageTransaction = true);
  RequirementResult setObsolete(int id);

private:
  QString m_connectionName;
};
#endif
