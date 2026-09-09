#ifndef PRODUCTTREEWIDGET_H
#define PRODUCTTREEWIDGET_H
#include <QWidget>
#include "producttreeservice.h"
class QCheckBox; class QLineEdit; class QPushButton; class QTableWidget; class QTreeWidget; class QTreeWidgetItem; class QTextEdit;
class ProductTreeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ProductTreeWidget(QWidget *p=nullptr);
    void setConnectionName(const QString&);
public slots:
    void refresh();
signals:
    void dataChanged();
private slots:
    void selectNode(QTreeWidgetItem*); void addRoot(); void addChild(); void save();
    void remove(); void archive(); void filter(const QString&);
private:
    QTreeWidgetItem* addItems(int,QTreeWidgetItem*); int selectedId()const;
    void showResult(const ProductTreeResult&);
    ProductTreeService m_service; QString m_connection; QTreeWidget*m_tree;
    QLineEdit*m_search,*m_segment,*m_code; QTextEdit*m_description;
    QPushButton*m_root,*m_child,*m_remove,*m_archive; QCheckBox*m_showArchived;
    QTableWidget*m_links,*m_metrics;
};
#endif
