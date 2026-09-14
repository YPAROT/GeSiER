#ifndef VERIFICATIONWIDGET_H
#define VERIFICATIONWIDGET_H

#include "verificationservice.h"
#include <QWidget>

class QCheckBox; class QComboBox; class QLabel; class QTableWidget;

class VerificationWidget : public QWidget {
  Q_OBJECT
public:
  explicit VerificationWidget(QWidget *parent=nullptr);
  void setConnectionName(const QString &name);
public slots:
  void refresh();
signals:
  void dataChanged();
  void openRequirementRequested(int id);
private:
  VerificationFilter filter() const;
  void refreshMatrix();
  void editRequirement(int requirementId);
  QString m_connection; VerificationService m_service;
  QComboBox *m_pt=nullptr,*m_document=nullptr,*m_status=nullptr,*m_type=nullptr,*m_method=nullptr,*m_coverage=nullptr;
  QCheckBox *m_obsolete=nullptr; QTableWidget *m_matrix=nullptr; QLabel *m_rate=nullptr;
  QList<VerificationMatrixRow> m_rows;
};
#endif
