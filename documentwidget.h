#ifndef DOCUMENTWIDGET_H
#define DOCUMENTWIDGET_H

#include "documentservice.h"
#include <QWidget>

class QListWidget;
class QTreeWidget;
class QLineEdit;
class QTextEdit;
class QComboBox;
class QTableWidget;

class DocumentWidget : public QWidget {
  Q_OBJECT
public:
  explicit DocumentWidget(QWidget *parent = nullptr);
  void setConnectionName(const QString &connectionName);
  void releaseDatabase();

public slots:
  void refresh();
  void openDocument(int id);

signals:
  void dataChanged();
  void openRequirement(int id);

private:
  void loadDocument(int id);
  void loadTree(int id);
  int selectedParent() const;
  QString m_connection;
  DocumentService m_service;
  int m_current = -1;
  QListWidget *m_documents;
  QTreeWidget *m_tree;
  QLineEdit *m_reference;
  QLineEdit *m_title;
  QLineEdit *m_template;
  QLineEdit *m_secondary;
  QTextEdit *m_description;
  QComboBox *m_type;
  QComboBox *m_pt;
  QTableWidget *m_metadata;
};

#endif
