#ifndef EDITTABLEDIALOG_H
#define EDITTABLEDIALOG_H

#include <QDialog>
class QAbstractButton;

namespace Ui {
class EditTableDialog;
}

class EditTableDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditTableDialog(QWidget *parent = nullptr);
    ~EditTableDialog();

    int execute(QString tableName, QString requete, bool tableNameIsEditable, QString infoLabel, bool groupByRequest);
    QString request();
    QString tableName();
private slots:
    void on_tableNameLineEdit_textChanged(const QString &);

    void on_tableRequestPlainTextEdit_textChanged();

    void on_buttonBox_clicked(QAbstractButton *button);

private:
    Ui::EditTableDialog *ui;
};

#endif // EDITTABLEDIALOG_H
