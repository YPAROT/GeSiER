#ifndef TRACEABILITYWIDGET_H
#define TRACEABILITYWIDGET_H

#include <QWidget>

class QComboBox;
class QGraphicsScene;
class QGraphicsView;
class QPushButton;
class QSpinBox;
class QTableWidget;

class TraceabilityWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TraceabilityWidget(QWidget *parent = nullptr);
    void setConnectionName(const QString &connectionName);

public slots:
    void refresh();

signals:
    void dataChanged();
    void statusMessage(const QString &message);

private slots:
    void refreshSelection();
    void addRelation();
    void removeRelation();

private:
    bool wouldCreateDecompositionCycle(int parentId, int childId) const;
    void refreshRequirements();
    void refreshRelationTypes();
    void refreshList(int requirementId);
    void refreshGraph(int requirementId);

    QString m_connectionName;
    QComboBox *m_requirement;
    QComboBox *m_target;
    QComboBox *m_relationType;
    QSpinBox *m_depth;
    QPushButton *m_add;
    QPushButton *m_remove;
    QTableWidget *m_relations;
    QGraphicsView *m_graph;
    QGraphicsScene *m_scene;
};

#endif // TRACEABILITYWIDGET_H
