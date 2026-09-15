#ifndef REQIFSERVICE_H
#define REQIFSERVICE_H

#include <QList>
#include <QMap>
#include <QString>

struct ReqIfAttribute {
  QString id, name, value, type;
};

struct ReqIfObject {
  QString externalId, code, title, description, type, status, parentExternalId;
  QStringList productTrees;
  QList<ReqIfAttribute> attributes;
};

struct ReqIfRelation {
  QString externalId, sourceExternalId, targetExternalId, type;
};

struct ReqIfReport {
  QList<ReqIfObject> objects;
  QList<ReqIfRelation> relations;
  QStringList warnings, errors;
  int creates = 0, updates = 0, duplicates = 0;
  bool valid() const { return errors.isEmpty(); }
  QString summary() const;
};

class ReqIfService {
public:
  explicit ReqIfService(QString connectionName = {});
  ReqIfReport preview(const QString &fileName) const;
  bool importFile(const QString &fileName, const ReqIfReport &report,
                  QString *error = nullptr) const;
  bool exportFile(const QString &fileName, QString *error = nullptr) const;

private:
  QString m_connectionName;
};

#endif
