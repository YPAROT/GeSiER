#include "importlogdialog.h"
#include "ui_importlogdialog.h"

ImportLogDialog::ImportLogDialog(QString logText, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ImportLogDialog)
{
    ui->setupUi(this);
    setWindowTitle("Rapport d'importation");
    ui->plainTextEdit->insertPlainText(logText);
}

ImportLogDialog::~ImportLogDialog()
{
    delete ui;
}
