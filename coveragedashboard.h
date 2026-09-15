#ifndef COVERAGEDASHBOARD_H
#define COVERAGEDASHBOARD_H
#include "coverageservice.h"
#include <QWidget>
class QCheckBox; class QComboBox; class QLineEdit; class QLabel; class QVBoxLayout;
class CoverageDashboard : public QWidget {
  Q_OBJECT
public: explicit CoverageDashboard(QWidget *parent=nullptr); void setConnectionName(const QString&);
public slots: void refresh();
signals: void statusMessage(const QString&); void navigateRequested(int,const QString&);
private: void loadFilters(); void clearContent(); CoverageScope scope() const;
  QString m_connection; CoverageService m_service;
  QComboBox *m_pt=nullptr,*m_document=nullptr,*m_configuration=nullptr,*m_status=nullptr,*m_type=nullptr;
  QLineEdit *m_search=nullptr; QCheckBox *m_obsolete=nullptr; QLabel *m_state=nullptr; QVBoxLayout *m_content=nullptr;
};
#endif
