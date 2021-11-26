#ifndef IMPORTREQUIREMENTDIALOG_H
#define IMPORTREQUIREMENTDIALOG_H

#include <QDialog>
#include <QMap>

namespace Ui {
class ImportRequirementDialog;
}

class ImportRequirementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportRequirementDialog(QStringList CSVHeaders,QWidget *parent = 0);
    ~ImportRequirementDialog();
    QMap<int, int> getCSVMapping();
    bool isPTfromReqCodeChecked();
    QString getSeparateur();
    bool isChapGenChecked();

public slots:
    void accept() override;

private:
    Ui::ImportRequirementDialog *ui;
};

#endif // IMPORTREQUIREMENTDIALOG_H
