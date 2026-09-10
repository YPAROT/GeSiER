#include "producttreemodel.h"
#include "producttreeservice.h"
#include <QColor>
#include <QMimeData>
#include <QSqlDatabase>
#include <QSqlQuery>
ProductTreeModel::ProductTreeModel(QObject *p)
    : QAbstractItemModel(p), m_root(new Node) {}
ProductTreeModel::~ProductTreeModel() {
  clear();
  delete m_root;
}
void ProductTreeModel::clear() {
  qDeleteAll(m_root->children);
  m_root->children.clear();
}
void ProductTreeModel::setConnectionName(const QString &n) {
  m_connection = n;
  reload();
}
void ProductTreeModel::setShowArchived(bool v) {
  m_showArchived = v;
  reload();
}
void ProductTreeModel::reload() {
  beginResetModel();
  clear();
  const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
  if (m_connection.isEmpty() || !db.isValid() || !db.isOpen()) {
    endResetModel();
    return;
  }
  QMap<int, Node *> map;
  ProductTreeService s(m_connection);
  QString sql = "SELECT ID,PARENT,SEGMENT,DESCRIPTION,ARCHIVED FROM PT";
  if (!m_showArchived)
    sql += " WHERE ARCHIVED=0";
  sql += " ORDER BY POSITION,SEGMENT";
  QSqlQuery q(sql, db);
  while (q.next()) {
    auto n = new Node;
    n->id = q.value(0).toInt();
    n->parentId = q.value(1).isNull() ? -1 : q.value(1).toInt();
    n->segment = q.value(2).toString();
    n->description = q.value(3).toString();
    n->archived = q.value(4).toBool();
    n->code = s.fullCode(n->id);
    map[n->id] = n;
  }
  for (auto n : map) {
    Node *p = map.value(n->parentId, m_root);
    n->parent = p;
    p->children << n;
  }
  endResetModel();
}
ProductTreeModel::Node *ProductTreeModel::node(const QModelIndex &i) const {
  return i.isValid() ? static_cast<Node *>(i.internalPointer()) : m_root;
}
QModelIndex ProductTreeModel::index(int r, int c, const QModelIndex &p) const {
  Node *n = node(p);
  return r >= 0 && r < n->children.size() ? createIndex(r, c, n->children[r])
                                          : QModelIndex();
}
QModelIndex ProductTreeModel::parent(const QModelIndex &i) const {
  Node *n = node(i);
  if (!n || n->parent == m_root)
    return {};
  Node *p = n->parent;
  return createIndex(p->parent->children.indexOf(p), 0, p);
}
int ProductTreeModel::rowCount(const QModelIndex &p) const {
  return p.column() > 0 ? 0 : node(p)->children.size();
}
int ProductTreeModel::columnCount(const QModelIndex &) const { return 2; }
QVariant ProductTreeModel::data(const QModelIndex &i, int role) const {
  if (!i.isValid())
    return {};
  Node *n = node(i);
  if (role == Qt::DisplayRole || role == Qt::EditRole)
    return i.column() == 0 ? (role == Qt::EditRole ? n->segment : n->code)
                           : n->description;
  if (role == Qt::UserRole)
    return n->id;
  if (role == Qt::ForegroundRole && n->archived)
    return QColor(Qt::gray);
  return {};
}
QVariant ProductTreeModel::headerData(int s, Qt::Orientation o,
                                      int role) const {
  return o == Qt::Horizontal && role == Qt::DisplayRole
             ? (s == 0 ? "Code produit" : "Description")
             : QVariant();
}
Qt::ItemFlags ProductTreeModel::flags(const QModelIndex &i) const {
  return i.isValid()
             ? QAbstractItemModel::flags(i) |
                   (i.column() == 0 ? Qt::ItemIsEditable : Qt::NoItemFlags) |
                   Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled
             : Qt::ItemIsDropEnabled;
}
bool ProductTreeModel::setData(const QModelIndex &i, const QVariant &v,
                               int role) {
  if (role != Qt::EditRole || !i.isValid() || i.column() != 0)
    return false;
  Node *n = node(i);
  emit renameRequested(n->id, v.toString());
  return true;
}
QStringList ProductTreeModel::mimeTypes() const {
  return {"application/x-gesier-pt"};
}
QMimeData *ProductTreeModel::mimeData(const QModelIndexList &l) const {
  auto *m = new QMimeData;
  if (!l.isEmpty())
    m->setData(mimeTypes().first(), QByteArray::number(id(l.first())));
  return m;
}
bool ProductTreeModel::dropMimeData(const QMimeData *m, Qt::DropAction a,
                                    int row, int, const QModelIndex &p) {
  if (a == Qt::IgnoreAction)
    return true;
  if (!m->hasFormat(mimeTypes().first()))
    return false;
  emit moveRequested(m->data(mimeTypes().first()).toInt(),
                     p.isValid() ? id(p) : -1, row < 0 ? rowCount(p) : row);
  return true;
}
Qt::DropActions ProductTreeModel::supportedDropActions() const {
  return Qt::MoveAction;
}
int ProductTreeModel::id(const QModelIndex &i) const {
  return i.isValid() ? node(i)->id : -1;
}
ProductTreeModel::Node *ProductTreeModel::find(int id, Node *n) const {
  for (auto c : n->children) {
    if (c->id == id)
      return c;
    if (auto f = find(id, c))
      return f;
  }
  return nullptr;
}
QModelIndex ProductTreeModel::indexForId(int id) const {
  Node *n = find(id, m_root);
  return n ? createIndex(n->parent->children.indexOf(n), 0, n) : QModelIndex();
}
