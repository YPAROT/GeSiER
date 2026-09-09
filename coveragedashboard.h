#ifndef COVERAGEDASHBOARD_H
#define COVERAGEDASHBOARD_H

#include <QWidget>
#include <QList>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

class CoverageDashboard : public QWidget
{
    Q_OBJECT
public:
    explicit CoverageDashboard(QWidget *parent = nullptr);
    void setConnectionName(const QString &connectionName);

public slots:
    void refresh();

signals:
    void statusMessage(const QString &message);

private:
    struct Metric { QString label; int covered = 0; int total = 0; QString detailsQuery; };
    QString requirementScope(const QString &alias = "R") const;
    int scalar(const QString &query, bool *ok = nullptr) const;
    void refreshFilters();
    void showMissing(int row, int column);

    QString m_connectionName;
    QComboBox *m_statusFilter;
    QCheckBox *m_includeObsolete;
    QPushButton *m_refreshButton;
    QLabel *m_scopeLabel;
    QTableWidget *m_table;
    QList<Metric> m_metrics;
};

#endif // COVERAGEDASHBOARD_H
