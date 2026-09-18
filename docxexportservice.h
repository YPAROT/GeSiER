#ifndef DOCXEXPORTSERVICE_H
#define DOCXEXPORTSERVICE_H

#include "documentservice.h"

#include <QStringList>

struct DocxTemplateValidation {
  bool valid = false;
  QStringList errors;
  QStringList warnings;
};

struct DocxExportRequest {
  int documentId = -1;
  QString outputPath;
  QString templatePath;
  QString author;
  QString version;
  QString title;
  QString draftLabel;
  bool publication = false;
};

struct DocumentExportRecord {
  int id = -1;
  int documentId = -1;
  QString kind;
  QString version;
  QString title;
  QString author;
  QString exportedAt;
  QString filePath;
  QString sha256;
  QString gedReference;
  QString gedLink;
};

class DocxExportService {
public:
  explicit DocxExportService(QString connectionName = {});
  DocxTemplateValidation validateTemplate(const QString &path,
                                          int documentId = -1) const;
  RequirementResult exportDocument(const DocxExportRequest &request);
  QList<DocumentExportRecord> history(int documentId) const;
  RequirementResult setGedInformation(int exportId, const QString &reference,
                                      const QString &link);
  bool verifyHash(int exportId, QString *error = nullptr) const;

private:
  QString m_connectionName;
};

#endif
