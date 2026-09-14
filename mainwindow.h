#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlTableModel>
//#include "dcl_sqlmanager.h"
#include "req_sqlmanager.h"
//#include "etiquette.h"
#include "sqltableform.h"

#include "tableviewmanager.h"

#include "sqltreemodel.h"
//#include "sqltreereqmodel.h"
#include "reqtrackform.h"

QT_FORWARD_DECLARE_CLASS(QTableView)
QT_FORWARD_DECLARE_CLASS(QPushButton)
QT_FORWARD_DECLARE_CLASS(QTextEdit)
QT_FORWARD_DECLARE_CLASS(QSqlError)
QT_FORWARD_DECLARE_CLASS(QTableWidgetItem)
class CoverageDashboard;
class TraceabilityWidget;
class N2MatrixWidget;
class ProductTreeWidget;
class RequirementWidget;
class DocumentWidget;
class ApplicabilityWidget;
class VerificationWidget;
class InterfaceWidget;
class QListWidget;
class QStackedWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE



//struct Etiquette_Data
//{
//    QStringList ParamNameList;
//    QStringList ParamValueList;
//    QVector<int> ParamPoliceList;
//};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
//    Etiquette_Data GetEtiquetteData(QString PN_str);

    void exportToCsv(QTableView *sqlTable, QString tableName);

private:
    bool hasOpenProject() const;
    void updateProjectUi();
    void closeProject();
    void releaseProjectViews();
    void bindProjectViews();
    void refreshViewTables();
    void refreshEditTables();
    QVariant getRelatedTablePKvalue(int column, SQLTableForm *tableform, QString searchStr, bool CanBeNull=false);
    void importReq();
    void importBasic(SQLTableForm *tableform);
    void importChapter(SQLTableForm *tableform);
    void setupModernNavigation();

private slots:

    void on_actionClose_triggered();

    void on_actionNew_DB_triggered();

    void on_actionLoad_DB_triggered();

    void on_submitButton_clicked();

    void on_clearButton_clicked();

    void on_actionSave_as_triggered();

    void on_customTableComboBox_currentIndexChanged(int index);

    void on_exportSqlToCsvPushButton_clicked();

    void on_refreshCustomTablePushButton_clicked();

    void on_exportCustomViewToCsvPushButton_clicked();

    void on_importTableConfPushButton_clicked();

    void on_exportTableConfPushButton_clicked();

    void tableViewUpdated();

    void on_editTabWidget_currentChanged(int index);

    void on_exportDocToCsvPushButton_clicked();

    void refreshTableSlot(void);

    void on_exportReqListToCsvPushButton_clicked();

    void on_importTablePushButton_clicked();

signals:

    void statusMessage(const QString &message);

private:
    Ui::MainWindow *ui;

    REQ_SQLManager* m_SQLManager;
    QList<QStringList> m_csvData;
    TableViewManager* m_tableViewManager;
    QString m_importLog;

    //Tree a refaire au besoin ailleurs
    SqlTreeModel* m_PTmodel = nullptr;
    CoverageDashboard* m_coverageDashboard = nullptr;
    TraceabilityWidget* m_traceabilityWidget = nullptr;
    N2MatrixWidget* m_n2MatrixWidget = nullptr;
    ProductTreeWidget* m_productTreeWidget = nullptr;
    RequirementWidget* m_requirementWidget = nullptr;
    DocumentWidget* m_documentWidget = nullptr;
    ApplicabilityWidget* m_applicabilityWidget = nullptr;
    VerificationWidget* m_verificationWidget = nullptr;
    InterfaceWidget* m_interfaceWidget = nullptr;
    QListWidget* m_projectNavigation = nullptr;
    QStackedWidget* m_projectPages = nullptr;
    //SqlTreeReqModel* m_Req_Arbo;
};


#endif // MAINWINDOW_H
