#ifndef DOCXIMPORTSERVICE_H
#define DOCXIMPORTSERVICE_H

#include "requirementservice.h"
#include <QList>
#include <QStringList>

struct DocxImportValidation {
  bool valid = false;
  QStringList errors;
  QStringList warnings;
};

struct DocxImportRelation {
  QString type;
  QString direction;
  QString otherCode;
  QString comment;
};

enum class DocxImportRecognition {
  Conformant,
  Candidate
};

struct DocxImportDiagnostic {
  QString location;
  QString field;
  QString message;
  bool blocking = true;
};

struct DocxImportRequirement {
  RequirementRecord record;
  QString type;
  QString status;
  QStringList productTreeCodes;
  QStringList configurationCodes;
  QStringList chapterPath;
  QList<DocxImportRelation> relations;
  QStringList verificationMethods;
  int ordinal = 0;
  QString location;
  QString importAction = "update";
  bool selected = true;
  DocxImportRecognition recognition = DocxImportRecognition::Conformant;
  QStringList diagnostics;
  QList<DocxImportDiagnostic> detailedDiagnostics;
};

struct DocxImportPreview {
  QList<DocxImportRequirement> requirements;
  QString documentReference;
  QString documentTitle;
  QStringList errors;
  QStringList warnings;
  bool valid() const { return errors.isEmpty() && !requirements.isEmpty(); }
};

struct DocxImportOptions {
  int documentId = -1;
  int primaryPtId = -1;
  QString documentReference;
  QString documentTitle;
  QString documentDescription;
  bool updateDuplicates = true;
  bool emptyValuesClear = true;
};

class DocxImportService {
public:
  explicit DocxImportService(QString connectionName = {});
  DocxImportValidation validateTemplate(const QString &path) const;
  DocxImportPreview preview(const QString &sourcePath,
                            const QString &templatePath) const;
  RequirementResult importPreview(const DocxImportPreview &preview,
                                  const DocxImportOptions &options) const;

private:
  QString m_connectionName;
};

#endif
