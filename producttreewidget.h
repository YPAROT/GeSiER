#ifndef PRODUCTTREEWIDGET_H
#define PRODUCTTREEWIDGET_H
#include "producttreeservice.h"
#include <QWidget>
class ProductTreeModel;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTreeView;
class QTextEdit;
class ProductTreeWidget : public QWidget {
  Q_OBJECT
public:
  explicit ProductTreeWidget(QWidget *p = nullptr);
  void setConnectionName(const QString &);
public slots:
  void refresh();
signals:
  void dataChanged();
  void openRequirementsForPt(int);
  void openInterfacesForPt(int);
  void openChangesForPt(int);
private slots:
  void selectNode();
  void addRoot();
  void addChild();
  void save();
  void remove();
  void archive();
  void moveRequested(int, int, int);
  void renameRequested(int, const QString &);

private:
  int selectedId() const;
  void showResult(const ProductTreeResult &);
  void populateLinks(int);
  void populateMetrics(int);
  ProductTreeService m_service;
  QString m_connection;
  ProductTreeModel *m_model;
  QTreeView *m_tree;
  QLineEdit *m_search, *m_segment, *m_code;
  QTextEdit *m_description;
  QPushButton *m_root, *m_child, *m_remove, *m_archive;
  QCheckBox *m_showArchived;
  QTableWidget *m_links, *m_metrics;
};
#endif
