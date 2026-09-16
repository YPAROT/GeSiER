#ifndef HISTORYWIDGET_H
#define HISTORYWIDGET_H

#include <QWidget>

class QComboBox;
class QLineEdit;
class QTableWidget;

class HistoryWidget : public QWidget {
  Q_OBJECT
public:
  explicit HistoryWidget(QWidget *parent = nullptr);
  void setConnectionName(const QString &connectionName);
  void refresh();
signals:
  void openObjectRequested(const QString &objectType, int objectId);
private:
  QString m_connectionName;
  QLineEdit *m_search = nullptr;
  QComboBox *m_object = nullptr;
  QComboBox *m_event = nullptr;
  QTableWidget *m_table = nullptr;
};

#endif
