#ifndef VERIFICATIONSERVICE_H
#define VERIFICATIONSERVICE_H

#include "requirementservice.h"
#include <QList>
#include <QString>

struct VerificationFilter {
  int ptRootId = -1, documentId = -1, statusId = -1, typeId = -1,
      methodId = -1;
  bool includeObsolete = false;
  // -1 tous, 0 manquante, 1 couverte, 2 couverte/draft, 3 draft, 4 tout draft
  int coverage = -1;
};

struct VerificationMatrixRow {
  int requirementId = -1;
  QString code, title, status, type, productTrees, documents, methods, levels;
  int verificationCount = 0;
  bool covered = false, draft = false;
  QString coverageLabel() const;
  QList<RequirementVerification> verifications;
};

class VerificationService {
public:
  explicit VerificationService(QString connectionName = {});
  void setConnectionName(const QString &name);
  QList<VerificationMatrixRow> matrix(const VerificationFilter &filter,
                                      QString *error = nullptr) const;
  RequirementResult save(int requirementId,
                         const QList<RequirementVerification> &rows);
  double coverageRate(const QList<VerificationMatrixRow> &rows) const;
  RequirementResult exportXlsx(const QString &path,
                               const VerificationFilter &filter) const;

private:
  QString m_connectionName;
};

#endif
