#ifndef PRODUCTTREEMODEL_H
#define PRODUCTTREEMODEL_H
#include <QAbstractItemModel>
class ProductTreeModel : public QAbstractItemModel {
  Q_OBJECT
public:
  explicit ProductTreeModel(QObject *p = nullptr);
  ~ProductTreeModel();
  void setConnectionName(const QString &);
  void setShowArchived(bool);
  void reload();
  int id(const QModelIndex &) const;
  QModelIndex indexForId(int) const;
  QModelIndex index(int, int,
                    const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &, int) const override;
  QVariant headerData(int, Qt::Orientation, int) const override;
  Qt::ItemFlags flags(const QModelIndex &) const override;
  bool setData(const QModelIndex &, const QVariant &, int) override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &) const override;
  bool dropMimeData(const QMimeData *, Qt::DropAction, int, int,
                    const QModelIndex &) override;
  Qt::DropActions supportedDropActions() const override;
signals:
  void renameRequested(int, const QString &);
  void moveRequested(int, int, int);

private:
  struct Node {
    ~Node() { qDeleteAll(children); }
    int id = -1, parentId = -1;
    QString code, segment, description;
    bool archived = false;
    Node *parent = nullptr;
    QList<Node *> children;
  };
  void clear();
  Node *node(const QModelIndex &) const;
  Node *find(int, Node *) const;
  QString m_connection;
  bool m_showArchived = false;
  Node *m_root;
};
#endif
