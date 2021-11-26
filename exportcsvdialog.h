#ifndef EXPORTCSVDIALOG_H
#define EXPORTCSVDIALOG_H

#include <QDialog>

namespace Ui {
class exportCsvDialog;
}

class exportCsvDialog : public QDialog
{
    Q_OBJECT

public:
    explicit exportCsvDialog(QWidget *parent = nullptr);
    ~exportCsvDialog();

    int exec(QStringList tableHeaders, QString tableName);
    QList<int> columnIndexes();
    QStringList columnNames();

public slots:
    void accept();

private:
    Ui::exportCsvDialog *ui;

    QString m_tableName;
};

#endif // EXPORTCSVDIALOG_H
