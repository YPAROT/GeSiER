#ifndef DOCUMENTSERVICE_H
#define DOCUMENTSERVICE_H

#include "requirementservice.h"

struct DocumentRecord {
  int id = -1;
  int ptId = -1;
  int typeId = -1;
  QString reference;
  QString title;
  QString description;
  QStringList secondaryReferences;
};

struct DocumentNodeRecord {
  int id = -1;
  int documentId = -1;
  int parentId = -1;
  int position = 0;
  QString type;
  QString title;
  int requirementId = -1;
  QString requirementCode;
  QString requirementTitle;
};

class DocumentService {
public:
  explicit DocumentService(QString connectionName = {});
  QList<DocumentRecord> documents() const;
  DocumentRecord document(int id) const;
  QList<DocumentNodeRecord> nodes(int documentId) const;
  RequirementResult saveDocument(const DocumentRecord &record);
  RequirementResult addChapter(int documentId, int parentId,
                               const QString &title);
  RequirementResult placeRequirement(int documentId, int parentId,
                                     int requirementId);
  RequirementResult moveNode(int nodeId, int parentId, int position);
  RequirementResult renameChapter(int nodeId, const QString &title);
  RequirementResult removeNode(int nodeId);

private:
  QString m_connectionName;
};

#endif
