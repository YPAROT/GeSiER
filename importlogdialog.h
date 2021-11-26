#ifndef IMPORTLOGDIALOG_H
#define IMPORTLOGDIALOG_H

#include <QDialog>

namespace Ui {
class ImportLogDialog;
}

class ImportLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportLogDialog(QString logText,QWidget *parent = 0);
    ~ImportLogDialog();

private:
    Ui::ImportLogDialog *ui;
};

#endif // IMPORTLOGDIALOG_H
