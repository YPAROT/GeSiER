#include "edittabledialog.h"
#include "ui_edittabledialog.h"

#include <QPushButton>

EditTableDialog::EditTableDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditTableDialog)
{
    ui->setupUi(this);
}

EditTableDialog::~EditTableDialog()
{
    delete ui;
}


int EditTableDialog::execute(QString tableName, QString requete, bool tableNameIsEditable, QString infoLabel,bool groupByRequest)
{    
    ui->tableNameLineEdit->setText(tableName);
    ui->tableNameLabel->setText(tableName);
    if (groupByRequest)
    {
        QStringList requests = requete.split("##");
        ui->groupByWidget->setVisible(true);
        ui->tableRequestPlainTextEdit->clear();
        ui->tableRequestPlainTextEdit->appendPlainText(requests.first());
        ui->groupByRequestPlainTextEdit->clear();
        ui->groupByRequestPlainTextEdit->appendPlainText(requests.last());
    }
    else
    {
        ui->groupByWidget->setVisible(false);
        ui->tableRequestPlainTextEdit->clear();
        ui->tableRequestPlainTextEdit->appendPlainText(requete);
        ui->groupByRequestPlainTextEdit->clear();
    }

    ui->tableNameLineEdit->setVisible(tableNameIsEditable);
    ui->tableNameLabel->setVisible(!tableNameIsEditable);
    ui->infoLabel->setText(infoLabel);
    ui->infoLabel->setVisible(!infoLabel.isEmpty());

    return exec();
}

QString EditTableDialog::tableName()
{
    return ui->tableNameLineEdit->text();
}

QString EditTableDialog::request()
{
    if (ui->groupByRequestPlainTextEdit->toPlainText().isEmpty())
        return ui->tableRequestPlainTextEdit->toPlainText();
    else
        return ui->tableRequestPlainTextEdit->toPlainText() + "##" + ui->groupByRequestPlainTextEdit->toPlainText();
}

void EditTableDialog::on_tableNameLineEdit_textChanged(const QString &)
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(!ui->tableNameLineEdit->text().isEmpty()
                                                                  && !ui->tableRequestPlainTextEdit->toPlainText().isEmpty());
}

void EditTableDialog::on_tableRequestPlainTextEdit_textChanged()
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(!ui->tableNameLineEdit->text().isEmpty()
                                                                  && !ui->tableRequestPlainTextEdit->toPlainText().isEmpty());

}

void EditTableDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    if (button == ui->buttonBox->button(QDialogButtonBox::Apply))
        accept();
}
