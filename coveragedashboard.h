#ifndef COVERAGEDASHBOARD_H
#define COVERAGEDASHBOARD_H
#include <QWidget>
class QCheckBox;
class QComboBox;
class QLabel;
class QGridLayout;
class CoverageDashboard : public QWidget {
  Q_OBJECT
public:
  explicit CoverageDashboard(QWidget *p = nullptr);
  void setConnectionName(const QString &);
public slots:
  void refresh();
signals:
  void statusMessage(const QString &);
  void navigateRequested(int, const QString &);

private:
  struct Metric {
    QString label;
    int covered = 0, total = 0, page = 1;
    QString filter;
  };
  int scalar(const QString &) const;
  QString scope() const;
  void filters();
  void clearCards();
  QString m_connection;
  QComboBox *m_status;
  QCheckBox *m_obsolete;
  QLabel *m_state;
  QGridLayout *m_grid;
  QList<Metric> m_metrics;
};
#endif
