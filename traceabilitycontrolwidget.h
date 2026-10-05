#ifndef TRACEABILITYCONTROLWIDGET_H
#define TRACEABILITYCONTROLWIDGET_H

#include "traceabilitycontrolservice.h"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;

class TraceabilityControlWidget : public QWidget {
  Q_OBJECT
public:
  explicit TraceabilityControlWidget(QWidget *parent = nullptr);
  void setConnectionName(const QString &connectionName);

public slots:
  void refresh();

signals:
  void openRequirementRequested(int requirementId);

private:
  void refreshStatuses();
  TraceabilityFilter filter() const;
  static void fillBooleanFilter(QComboBox *combo, const QString &allLabel);

  QString m_connectionName;
  TraceabilityControlService m_service;
  QLineEdit *m_search;
  QComboBox *m_status;
  QComboBox *m_applicable;
  QComboBox *m_takenIntoAccount;
  QComboBox *m_rootConform;
  QCheckBox *m_includeObsolete;
  QLabel *m_state;
  QTableWidget *m_table;
};

#endif
