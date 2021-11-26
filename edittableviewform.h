#ifndef EDITTABLEVIEWFORM_H
#define EDITTABLEVIEWFORM_H

#include <QWidget>

#include "tableviewmanager.h"

namespace Ui {
class EditTableViewForm;
}

class EditTableViewForm : public QWidget
{
    Q_OBJECT

public:
    explicit EditTableViewForm(QWidget *parent = nullptr);
    ~EditTableViewForm();
    void setTableViewManager(TableViewManager* tableViewManager) {m_tableViewManager = tableViewManager; updateTableView();}
private slots:
    void on_addTablePushButton_clicked();
    void on_editTableViewTableWidget_cellDoubleClicked(int row, int column);
    void deleteRow();

signals:
    void tableViewUpdated();
private:
    Ui::EditTableViewForm *ui;
    TableViewManager* m_tableViewManager;
    QAction *m_deleteRowAction;

    void updateTableView();
};

#endif // EDITTABLEVIEWFORM_H
