#ifndef CHANGEWIDGET_H
#define CHANGEWIDGET_H
#include "changeservice.h"
#include <QWidget>
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class ChangeWidget : public QWidget {
  Q_OBJECT
public:
  explicit ChangeWidget(QWidget *p = nullptr);
  void setConnectionName(const QString &);
  void applyObjectFilter(const QString &, int);
  void openChange(int);
public slots:
  void refresh();
signals:
  void dataChanged();
  void openRequirementRequested(int);
  void openProductTreeRequested(int);
  void openConfigurationRequested(int);
  void openInterfaceRequested(int);
  void openDocumentRequested(int);

private:
  void edit(int = -1);
  void catalog(const QString &);
  void showDetails();
  ChangeFilter filter() const;
  ChangeService m_service;
  QString m_connection, m_objectType;
  int m_objectId = -1;
  QList<ChangeRecord> m_rows;
  QLineEdit *m_search;
  QComboBox *m_type, *m_status;
  QCheckBox *m_archived, *m_incomplete;
  QTableWidget *m_table;
  QLabel *m_details;
  QLabel *m_summary;
  QPushButton *m_openLink;
};
#endif
