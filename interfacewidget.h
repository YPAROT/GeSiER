#ifndef INTERFACEWIDGET_H
#define INTERFACEWIDGET_H

#include "interfaceservice.h"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QTableWidget;
class QTabWidget;
class N2MatrixWidget;

class InterfaceWidget : public QWidget {
  Q_OBJECT
public:
  explicit InterfaceWidget(QWidget *parent = nullptr);
  void setConnectionName(const QString &name);
public slots:
  void refresh();
  void applyPtFilter(int ptId);
  void openInterface(int id);
signals:
  void dataChanged();
  void openProductTreeRequested(int id);
  void openRequirementRequested(int id);
  void openDocumentRequested(int id);
  void openChangesRequested(int id);

private:
  InterfaceFilter filter() const;
  void edit(int id = -1);
  void manageTypes();
  QString m_connection;
  InterfaceService m_service;
  QLineEdit *m_search = nullptr;
  QComboBox *m_pt = nullptr, *m_type = nullptr;
  QCheckBox *m_archived = nullptr, *m_withoutIcd = nullptr;
  QLabel *m_summary = nullptr;
  QTableWidget *m_table = nullptr;
  QTabWidget *m_tabs = nullptr;
  N2MatrixWidget *m_n2 = nullptr;
  QList<InterfaceRecord> m_rows;
};

#endif
