#ifndef IMPORTBASICDIALOG_H
#define IMPORTBASICDIALOG_H

#include <QDialog>

namespace Ui {
class ImportBasicDialog;
}

class ImportBasicDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportBasicDialog(QString tableName, QStringList TableHeaders, QStringList CSVHeaders, QWidget *parent = 0);
    ~ImportBasicDialog();
    QMap<int, int> getCSVMapping();

public slots:
    void accept() override;

private:
    Ui::ImportBasicDialog *ui;
    QString m_tableName;
};

#endif // IMPORTBASICDIALOG_H
