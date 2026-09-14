#ifndef DOCUMENTSERVICE_H
#define DOCUMENTSERVICE_H

#include "requirementservice.h"
#include <QByteArray>
#include <QMap>

struct DocumentRecord {
  int id = -1;
  int ptId = -1;
  int typeId = -1;
  QString reference;
  QString title;
  QString description;
  QString templatePath;
  QStringList secondaryReferences;
  QMap<QString, QString> metadata;
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
  QString requirementDescription;
  QString textContent;
  QByteArray imageData;
  QString imageLegend;
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
  RequirementResult addText(int documentId, int parentId,
                            const QString &html);
  RequirementResult addImage(int documentId, int parentId,
                             const QByteArray &data, const QString &legend);
  RequirementResult placeRequirement(int documentId, int parentId,
                                     int requirementId);
  RequirementResult moveNode(int nodeId, int parentId, int position);
  RequirementResult renameChapter(int nodeId, const QString &title);
  RequirementResult updateText(int nodeId, const QString &html);
  RequirementResult updateImage(int nodeId, const QByteArray &data,
                                const QString &legend);
  RequirementResult removeNode(int nodeId);
  QString previewHtml(int documentId) const;

private:
  QString m_connectionName;
};

#endif
