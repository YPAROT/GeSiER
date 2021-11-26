#ifndef REQTRACKFORM_H
#define REQTRACKFORM_H

#include <QWidget>
#include <QtSql>
#include "sqltreereqmodel.h"

namespace Ui {
class ReqTrackForm;
}

class ReqTrackForm : public QWidget
{
    Q_OBJECT

public:
    explicit ReqTrackForm(QWidget *parent = Q_NULLPTR);
    ~ReqTrackForm();

    void setConnectionName(QString connectionName);
    bool init(void);
    bool refresh(void);

private slots:
    void on_ReqcomboBox_currentIndexChanged(int index);

private:
    bool fillComboBox(QSqlDatabase db);
    bool SetModel(QModelIndex idx, QSqlDatabase db);

private:
    Ui::ReqTrackForm *ui;
    QSqlTableModel *m_req_model;
    SqlTreeReqModel *m_Req_Arbo;
    QString m_connectionName;
};

#endif // REQTRACKFORM_H
