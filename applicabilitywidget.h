#ifndef APPLICABILITYWIDGET_H
#define APPLICABILITYWIDGET_H
#include "applicabilityservice.h"
#include <QWidget>
class QCheckBox; class QComboBox; class QLabel; class QPushButton; class QTableWidget;
class ApplicabilityWidget : public QWidget {
  Q_OBJECT
public:
  explicit ApplicabilityWidget(QWidget *parent=nullptr);
  void setConnectionName(const QString &name);
public slots: void refresh();
signals: void dataChanged(); void openRequirementRequested(int id);
private:
  void editConfiguration(int id=-1); void refreshConfigurations(); void refreshMatrix();
  QString m_connection; ApplicabilityService m_service;
  QTableWidget *m_configurations,*m_matrix,*m_history;
  QComboBox *m_pt,*m_document,*m_status,*m_type;
  QCheckBox *m_obsolete, *m_archivedConfigurations; QLabel *m_rate; QPushButton *m_archive;
  QList<ConfigurationRecord> m_configurationRecords; QList<ApplicabilityRow> m_rows;
};
#endif
