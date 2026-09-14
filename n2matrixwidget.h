#ifndef N2MATRIXWIDGET_H
#define N2MATRIXWIDGET_H

#include <QList>
#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

class N2MatrixWidget : public QWidget {
  Q_OBJECT
public:
  explicit N2MatrixWidget(QWidget *parent = nullptr);
  void setConnectionName(const QString &connectionName);

public slots:
  void refresh();

signals:
  void pairSelected(int firstPtId, int secondPtId);
  void interfaceRequested(int interfaceId);

private:
  void showDetails(int row, int column);
  QString m_connectionName;
  QTableWidget *m_matrix;
  QCheckBox *m_onlyWithInterfaces;
  QPushButton *m_refresh;
  QLabel *m_rate = nullptr;
  QList<int> m_ptIds;
};

#endif // N2MATRIXWIDGET_H
