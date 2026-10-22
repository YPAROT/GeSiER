#ifndef REQUIREMENTCODEFORMAT_H
#define REQUIREMENTCODEFORMAT_H

#include <QString>
#include <QStringList>

struct RequirementCodeFormat {
  QChar separator = '-';
  QString marker = "R";
  int digits = 4;
  bool includeRoot = true;
};

struct RequirementCodeValidation {
  bool conforming = false;
  int index = -1;
  QStringList errors;
};

struct RequirementCodeFormatResult {
  bool success = false;
  QString message;
  static RequirementCodeFormatResult ok() { return {true, {}}; }
  static RequirementCodeFormatResult failure(const QString &message) {
    return {false, message};
  }
};

class RequirementCodeFormatService {
public:
  explicit RequirementCodeFormatService(QString connectionName = {});
  void setConnectionName(const QString &connectionName);
  RequirementCodeFormat format() const;
  RequirementCodeFormatResult saveFormat(const RequirementCodeFormat &format) const;
  QString productTreePath(int ptId, bool applyRootSetting = true) const;
  QString prefix(int ptId) const;
  QString code(int ptId, int index) const;
  RequirementCodeValidation validate(const QString &code, int ptId) const;

private:
  QString m_connectionName;
};

#endif
