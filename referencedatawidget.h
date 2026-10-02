#ifndef REFERENCEDATAWIDGET_H
#define REFERENCEDATAWIDGET_H

#include "referencedataservice.h"
#include <QWidget>

class QLineEdit;
class QTableWidget;

class ReferenceDataWidget : public QWidget {
  Q_OBJECT
public:
  explicit ReferenceDataWidget(QWidget *parent = nullptr);
  void setConnectionName(const QString &name);
  void releaseDatabase();
public slots:
  void refresh();
signals:
  void dataChanged();
private:
  void refreshMethods();
  void refreshTypes();
  void showResult(const ReferenceDataResult &result);
  int selectedId(QTableWidget *table) const;
  QString m_connection;
  ReferenceDataService m_service;
  QTableWidget *m_methods = nullptr;
  QTableWidget *m_types = nullptr;
  QLineEdit *m_methodLabel = nullptr;
  QLineEdit *m_typeCode = nullptr;
  QLineEdit *m_typeLabel = nullptr;
};

#endif
