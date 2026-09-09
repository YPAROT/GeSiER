#ifndef PRODUCTTREESERVICE_H
#define PRODUCTTREESERVICE_H
#include <QString>
#include <QStringList>
struct ProductTreeResult { bool success=false; QString message; QStringList affectedCodes; QStringList blockers; };
class ProductTreeService {
public:
    explicit ProductTreeService(QString connectionName={});
    void setConnectionName(const QString &name);
    ProductTreeResult addNode(int parentId,const QString &segment);
    ProductTreeResult updateNode(int id,const QString &segment,const QString &description);
    ProductTreeResult moveNode(int id,int parentId,int position);
    ProductTreeResult setArchived(int id,bool archived);
    ProductTreeResult removeNode(int id);
    QString fullCode(int id) const;
    QStringList subtreeCodes(int id) const;
private:
    ProductTreeResult fail(const QString &message) const;
    QString m_connectionName;
};
#endif
