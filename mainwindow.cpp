#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "requirementimportdialog.h"
#include "applicationlanguage.h"
#include <QFileDialog>
#include <QSettings>
#include <QSqlError>
#include <QMessageBox>
#include <QtSql>
#include <qwidgetsettings.h>
#include <QStandardItemModel>
#include "exportcsvdialog.h"
#include "importbasicdialog.h"
#include "importlogdialog.h"
#include "coveragedashboard.h"
#include "traceabilitywidget.h"
#include "traceabilitycontrolwidget.h"
#include "projectpages.h"
#include "n2matrixwidget.h"
#include "producttreewidget.h"
#include "requirementwidget.h"
#include "documentwidget.h"
#include "applicabilitywidget.h"
#include "verificationwidget.h"
#include "interfacewidget.h"
#include "changewidget.h"
#include "historywidget.h"
#include "referencedatawidget.h"
#include "historyservice.h"
#include "reqifservice.h"
#include <QAction>
#include <QActionGroup>
#include <QMenu>

#include "csvutility.h"

#include <QTableWidget> //ajout après suppression etiquette
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    auto *languageMenu = ui->menubar->addMenu(tr("Langue"));
    auto *languageGroup = new QActionGroup(languageMenu);
    languageGroup->setExclusive(true);
    auto addLanguage = [&](const QString &label, const QString &code) {
        auto *action = languageMenu->addAction(label);
        action->setCheckable(true);
        action->setData(code);
        action->setChecked(ApplicationLanguage::current() == code);
        languageGroup->addAction(action);
    };
    addLanguage(tr("Français"), "fr");
    addLanguage(tr("English"), "en");
    connect(languageGroup, &QActionGroup::triggered, this, [](QAction *action) {
        ApplicationLanguage::setCurrent(action->data().toString());
    });
    m_coverageDashboard = new CoverageDashboard(this);
    m_traceabilityWidget = new TraceabilityWidget(this);
    ui->ViewTabWidget->addTab(m_traceabilityWidget, tr("Traçabilité"));
    m_n2MatrixWidget = new N2MatrixWidget(this);
    ui->ViewTabWidget->addTab(m_n2MatrixWidget, tr("Matrice N²"));
    connect(m_coverageDashboard, &CoverageDashboard::statusMessage,
            this, &MainWindow::statusMessage);
    connect(m_traceabilityWidget, &TraceabilityWidget::dataChanged, this,
            &MainWindow::refreshTableSlot);
    m_SQLManager = new REQ_SQLManager();
    m_tableViewManager = new TableViewManager();
    m_tableViewManager->loadConfFromFile(QApplication::applicationDirPath()+"\\defaultConf.ini");
    ui->editTableViewWidget->setTableViewManager(m_tableViewManager);
    connect(ui->editTableViewWidget,SIGNAL(tableViewUpdated()),this,SLOT(tableViewUpdated()));
    ui->customTableComboBox->addItems(m_tableViewManager->tablesNames());
    ui->stackedWidget->setCurrentIndex(0);
    ui->mainTabWidget->setCurrentIndex(0);
    ui->ViewTabWidget->setCurrentIndex(0);
    ui->editTabWidget->setCurrentIndex(0);

    // A commit in any editable reference table can affect several screens
    // (labels, filters, coverage, trees, ...). Keep this list in one place and
    // install the connections even when the application starts without a
    // project open.
    const QList<SQLTableForm *> editableTables = {
        ui->editPTSqlTableWidget,       ui->editReqSqlTableWidget,
        ui->editreqmethodSqlTableWidget, ui->editreqstatusSqlTableWidget,
        ui->editreqtypeSqlTableWidget, ui->editDocumentSqlTableWidget,
        ui->editDocTypeSqlTableWidget, ui->editReqChapterSqlTableWidget,
        ui->editConfChangeWidget, ui->editIFWidget,
        ui->editIFChaptersWidget, ui->editModelWidget};
    for (SQLTableForm *table : editableTables)
        connect(table, &SQLTableForm::comitOccured, this,
                &MainWindow::refreshTableSlot);


    QString filename = qEnvironmentVariable("GESIER_PROJECT_PATH");
    if (filename.isEmpty() && !qEnvironmentVariableIsSet("GESIER_IGNORE_LAST_PROJECT"))
        filename = QWidgetSettings::value("REQ").toString();
    if (!filename.isEmpty())
    {
        QSqlError err = m_SQLManager->openDB(filename);
        if (err.type() == QSqlError::NoError)
        {
            setWindowTitle("GEstion  SImplifée d'Exigences pour La Recherche: "+filename);

            //treeview test
            m_PTmodel = new SqlTreeModel(m_SQLManager->currentConnection(),this);
            m_PTmodel->setRelation(QSqlRelation("PT","ID","PARENT"));
            QMap<int,int> PTmap;
            PTmap.insert(0,1);
            PTmap.insert(1,2);
            PTmap.insert(2,0);
            m_PTmodel->setColumnMapping(PTmap);
            m_PTmodel->select();
            ui->PTtreeView->setModel(m_PTmodel);
            ui->PTtreeView->hideColumn(1);
            ui->PTtreeView->hideColumn(2);
            ui->PTtreeView->expandAll();
            //fin de test

            //test pour arborescence requirement
            ui->ReqTrackViewWidget->setConnectionName(m_SQLManager->currentConnection());
            ui->ReqTrackViewWidget->init();
            m_coverageDashboard->setConnectionName(m_SQLManager->currentConnection());
            m_traceabilityWidget->setConnectionName(m_SQLManager->currentConnection());
            m_n2MatrixWidget->setConnectionName(m_SQLManager->currentConnection());
            if (m_requirementWidget) m_requirementWidget->setConnectionName(m_SQLManager->currentConnection());
            if (m_traceabilityControlWidget) m_traceabilityControlWidget->setConnectionName(m_SQLManager->currentConnection());
            if (m_documentWidget) m_documentWidget->setConnectionName(m_SQLManager->currentConnection());


            refreshEditTables();

            refreshViewTables();
        }
    }

    setupModernNavigation();

    emit statusMessage(tr("Ready."));
}

MainWindow::~MainWindow()
{
    closeProject();
    delete m_PTmodel;
    m_PTmodel = nullptr;
    delete ui;
    ui = nullptr;
    delete m_SQLManager;
    m_tableViewManager->saveConfToFile(QApplication::applicationDirPath()+"\\defaultConf.ini");
    delete m_tableViewManager;
}

bool MainWindow::hasOpenProject() const
{
    if (!m_SQLManager)
        return false;
    const QString connection = m_SQLManager->currentConnection();
    return QSqlDatabase::contains(connection) &&
           QSqlDatabase::database(connection, false).isOpen();
}

void MainWindow::updateProjectUi()
{
    const bool open = hasOpenProject();
    ui->actionSave_as->setEnabled(open);
    if (m_projectNavigation)
        m_projectNavigation->setEnabled(open);
    if (m_projectPages)
        m_projectPages->setEnabled(open);
    // Le tableau de bord reste visible sans projet afin d'afficher son état
    // vide et l'action attendue. Seules les pages métier sont désactivées.
    if (!open)
        emit statusMessage(tr("Aucun projet ouvert. Créez ou ouvrez un projet pour commencer."));
}

void MainWindow::releaseProjectViews()
{
    if (!ui)
        return;
    ui->PTtreeView->setModel(nullptr);
    delete m_PTmodel;
    m_PTmodel = nullptr;
    ui->ReqTrackViewWidget->releaseDatabase();
    const QList<SQLTableForm *> forms = findChildren<SQLTableForm *>();
    for (SQLTableForm *form : forms)
        form->releaseDatabase();
    if (m_productTreeWidget) m_productTreeWidget->setConnectionName({});
    if (m_requirementWidget) m_requirementWidget->setConnectionName({});
    if (m_traceabilityControlWidget) m_traceabilityControlWidget->setConnectionName({});
    if (m_documentWidget) m_documentWidget->releaseDatabase();
    if (m_applicabilityWidget) m_applicabilityWidget->setConnectionName({});
    if (m_verificationWidget) m_verificationWidget->setConnectionName({});
    if (m_interfaceWidget) m_interfaceWidget->setConnectionName({});
    if (m_changeWidget) m_changeWidget->setConnectionName({});
    if (m_historyWidget) m_historyWidget->setConnectionName({});
    if (m_referenceDataWidget) m_referenceDataWidget->releaseDatabase();
    if (m_coverageDashboard) m_coverageDashboard->setConnectionName({});
    if (m_traceabilityWidget) m_traceabilityWidget->setConnectionName({});
    if (m_n2MatrixWidget) m_n2MatrixWidget->setConnectionName({});
}

void MainWindow::closeProject()
{
    releaseProjectViews();
    if (m_SQLManager)
        m_SQLManager->close();
    updateProjectUi();
}

void MainWindow::bindProjectViews()
{
    if (!ui || !m_SQLManager ||
        !hasOpenProject())
        return;
    const QString connection = m_SQLManager->currentConnection();
    ui->ReqTrackViewWidget->setConnectionName(connection);
    ui->ReqTrackViewWidget->init();
    if (m_coverageDashboard) m_coverageDashboard->setConnectionName(connection);
    if (m_traceabilityWidget) m_traceabilityWidget->setConnectionName(connection);
    if (m_n2MatrixWidget) m_n2MatrixWidget->setConnectionName(connection);
    if (m_productTreeWidget) m_productTreeWidget->setConnectionName(connection);
    if (m_requirementWidget) m_requirementWidget->setConnectionName(connection);
    if (m_traceabilityControlWidget) m_traceabilityControlWidget->setConnectionName(connection);
    if (m_documentWidget) m_documentWidget->setConnectionName(connection);
    if (m_applicabilityWidget) m_applicabilityWidget->setConnectionName(connection);
    if (m_verificationWidget) m_verificationWidget->setConnectionName(connection);
    if (m_interfaceWidget) m_interfaceWidget->setConnectionName(connection);
    if (m_changeWidget) m_changeWidget->setConnectionName(connection);
    if (m_historyWidget) m_historyWidget->setConnectionName(connection);
    if (m_referenceDataWidget) m_referenceDataWidget->setConnectionName(connection);
    HistoryService(connection).setAuthor(qEnvironmentVariable("USERNAME", "Utilisateur"));
    refreshEditTables();
    refreshViewTables();
    updateProjectUi();
}

void MainWindow::on_actionClose_triggered()
{
    closeProject();
    close();
}

void  MainWindow::tableViewUpdated()
{
    qDebug() << __FUNCTION__ << m_tableViewManager->tablesNames();

    QString currentText = ui->customTableComboBox->currentText();
    int currentIndex = ui->customTableComboBox->currentIndex();
    ui->customTableComboBox->clear();
    qDebug() << __FUNCTION__;
    ui->customTableComboBox->addItem("Customiser les vues");
    qDebug() << __FUNCTION__;
    ui->customTableComboBox->addItems(m_tableViewManager->tablesNames());
    qDebug() << __FUNCTION__;
    if (m_tableViewManager->tablesNames().indexOf(currentText)>=0)
        ui->customTableComboBox->setCurrentIndex(m_tableViewManager->tablesNames().indexOf(currentText));
    else if (currentIndex < ui->customTableComboBox->count())
        ui->customTableComboBox->setCurrentIndex(currentIndex);
    on_customTableComboBox_currentIndexChanged(ui->customTableComboBox->currentIndex());
}

void MainWindow::on_actionNew_DB_triggered()
{
    QString filename = QFileDialog::getSaveFileName(NULL, "Nom de la base de donnée de Requirements","","*.db");
    if (!filename.isEmpty())
    {
        if ((!QFile::exists(filename)) || (QMessageBox::information(NULL,"Nouvelle Base de données","Le fichier existe déjà, la BDD contenue dans le fichier va etre effacée, continuer ?",QMessageBox::Yes | QMessageBox::No)==QMessageBox::Yes))
        {
            const QString previousProject = m_SQLManager->filename();
            closeProject();
            QSqlError err = m_SQLManager->newDB(filename);
            if (err.type() != QSqlError::NoError)
            {
                QMessageBox::warning(this, tr("Unable to create database"), tr("An error occurred while "
                                                                               "creating the connection: ") + err.text());
                if (!previousProject.isEmpty() && m_SQLManager->openDB(previousProject).type() == QSqlError::NoError)
                    bindProjectViews();
            }
            else
            {
                QWidgetSettings::setValue("REQ",filename);
                setWindowTitle("GEstion  SImplifée d'Exigences pour La Recherche: "+filename);
                bindProjectViews();
            }
        }
    }
    updateProjectUi();
}

void MainWindow::on_actionLoad_DB_triggered()
{
    QString filename = QFileDialog::getOpenFileName(this,"Ouvrir une base de données",QApplication::applicationDirPath(),"*.db");
    if (!filename.isEmpty())
    {
        const QString previousProject = m_SQLManager->filename();
        closeProject();
        QSqlError err = m_SQLManager->openDB(filename);
        if (err.type() != QSqlError::NoError)
        {
            QMessageBox::warning(this, tr("Unable to open database"), tr("An error occurred while "
                                                                         "opening the connection: ") + err.text());
            if (!previousProject.isEmpty() && m_SQLManager->openDB(previousProject).type() == QSqlError::NoError)
                bindProjectViews();
        }
        else
        {
            QWidgetSettings::setValue("REQ",filename);
            setWindowTitle("GEstion  SImplifée d'Exigences pour La Recherche: "+filename);
            bindProjectViews();
        }
    }
    updateProjectUi();
}

void MainWindow::on_actionSave_as_triggered()
{
    QString filename = QFileDialog::getSaveFileName(this,"Enregistrer sous...","","*.db");
    if (!filename.isEmpty())
    {
        releaseProjectViews();
        if (m_SQLManager->saveAs(filename) == false)
            QMessageBox::warning(this,"Enregistrer sous...","Erreur lors du renommage du fichier");
        else
        {
            QWidgetSettings::setValue("REQ",filename);
            setWindowTitle("GEstion  SImplifée d'Exigences pour La Recherche: "+filename);
        }
        if (hasOpenProject())
            bindProjectViews();
    }
    updateProjectUi();
}

void MainWindow::refreshViewTables()
{
      // Les vues SQL historiques ne font plus partie de la navigation active.
      return;

      if (!m_SQLManager)
          return;
      const QString connection = m_SQLManager->currentConnection();
      if (connection.isEmpty() || !QSqlDatabase::contains(connection))
          return;
      const QSqlDatabase db = QSqlDatabase::database(connection, false);
      if (!db.isValid() || !db.isOpen())
          return;
      ui->docViewSqlTableWidget->execQuery("SELECT D.TITLE AS [Titre],D.DESCRIPTION AS [Description],COUNT(DISTINCT CASE WHEN N.NODE_TYPE='REQUIREMENT' THEN N.REQ_ID END) AS [Nombre de Requirements],COUNT(DISTINCT CASE WHEN N.NODE_TYPE='CHAPTER' THEN N.ID END) AS [Nombre de Chapitres] FROM DOCUMENT D LEFT JOIN DOCUMENT_NODE N ON N.DOC_ID=D.ID GROUP BY D.ID,D.TITLE,D.DESCRIPTION",m_SQLManager->currentConnection());

      ui->docIFViewSqlTableWidget->execQuery("SELECT D.TITLE AS [Titre],D.DESCRIPTION AS [Description],COUNT(DISTINCT L.INTERFACE_ID) AS [Nombre d'Interfaces],COUNT(DISTINCT CASE WHEN N.NODE_TYPE='CHAPTER' THEN N.ID END) AS [Nombre de Chapitres] FROM DOCUMENT D LEFT JOIN INTERFACE_DOCUMENT L ON L.DOC_ID=D.ID LEFT JOIN DOCUMENT_NODE N ON N.DOC_ID=D.ID GROUP BY D.ID,D.TITLE,D.DESCRIPTION",m_SQLManager->currentConnection());

      ui->ReqViewSqlTableWidget->execQuery("SELECT R.CODE AS [Req. Code],R.TITLE AS [Titre],COALESCE((SELECT GROUP_CONCAT(P.NAME,', ') FROM REQUIREMENT_PT A JOIN PT P ON P.ID=A.PT_ID WHERE A.REQ_ID=R.ID),'') AS [Elements du PT],COALESCE((SELECT GROUP_CONCAT(D.TITLE,', ') FROM DOCUMENT_NODE N JOIN DOCUMENT D ON D.ID=N.DOC_ID WHERE N.REQ_ID=R.ID),'') AS [Documents associés],R.DESCRIPTION AS [Description],RT.TYPE AS [Type],RS.STATUS AS [Statut],R.SOURCE AS [Source],COALESCE((SELECT GROUP_CONCAT(P.CODE,', ') FROM REQUIREMENT_RELATION L JOIN REQUIREMENT_RELATION_TYPE LT ON LT.ID=L.TYPE_ID JOIN REQUIREMENT P ON P.ID=L.SOURCE_REQ_ID WHERE L.TARGET_REQ_ID=R.ID AND LT.CODE='DECOMPOSE'),'') AS [Parents],COALESCE((SELECT GROUP_CONCAT(M.METHOD,', ') FROM REQUIREMENT_VERIFICATION V JOIN REQ_METHOD M ON M.ID=V.METHOD_ID WHERE V.REQ_ID=R.ID),'') AS [Méthodes de vérification],R.COMMENTS AS [Commentaires] FROM REQUIREMENT R LEFT JOIN REQ_TYPE RT ON RT.ID=R.TYPE LEFT JOIN REQ_STATUS RS ON RS.ID=R.STATUS",m_SQLManager->currentConnection());

      ui->ReqTrackViewWidget->refresh();

      ui->ConfChangeWidget->execQuery("SELECT C.CODE AS [Code],C.DESCRIPTION AS [Description],S.LABEL AS [Statut],COALESCE((SELECT GROUP_CONCAT(R.CODE,', ') FROM CHANGE_LINK L JOIN REQUIREMENT R ON L.OBJECT_TYPE='REQUIREMENT' AND R.ID=L.OBJECT_ID WHERE L.CHANGE_ID=C.ID),'') AS [Exigences] FROM CHANGE_ITEM C JOIN CHANGE_STATUS S ON S.ID=C.STATUS_ID",m_SQLManager->currentConnection());

      ui->IFWidget->execQuery("SELECT PT1.NAME AS [Element 1], PT2.NAME AS [Element 2], COALESCE((SELECT GROUP_CONCAT(D.TITLE,', ') FROM INTERFACE_DOCUMENT L JOIN DOCUMENT D ON D.ID=L.DOC_ID WHERE L.INTERFACE_ID=IF.ID),'') AS [Documents associés], IF.DESCRIPTION AS [Description] "
                              "FROM INTERFACE IF "
                              "LEFT JOIN PT AS PT1 ON IF.ELEMENT1=PT1.ID "
                              "LEFT JOIN PT AS PT2 ON IF.ELEMENT2=PT2.ID",m_SQLManager->currentConnection());

      ui->ModelWidget->execQuery("SELECT CODE AS [Modèle Instrument],LABEL AS [Libellé] FROM CONFIGURATION WHERE ACTIVE=1 ORDER BY POSITION,CODE",m_SQLManager->currentConnection());

      ui->LogsWidget->execQuery("SELECT EVENT_TIME AS [Date],EVENT_TYPE AS [Type],OBJECT_TYPE AS [Objet],OBJECT_ID AS [Identifiant],AUTHOR AS [Auteur],BEFORE_JSON AS [Avant],AFTER_JSON AS [Après],COMMENT AS [Commentaire] FROM EVENT_LOG ORDER BY ID DESC",m_SQLManager->currentConnection());


}

void MainWindow::on_submitButton_clicked()
{
    ui->manualSqlTableWidget->execQuery(ui->sqlEdit->toPlainText(), m_SQLManager->currentConnection());
    ui->sqlEdit->setFocus();
}

void MainWindow::on_clearButton_clicked()
{
    ui->sqlEdit->clear();
    ui->sqlEdit->setFocus();
}



void MainWindow::refreshEditTables()
{
    // Les écritures doivent passer par les services métier afin de préserver
    // les validations, transactions et événements d'audit.
    return;

    if (!hasOpenProject())
        return;
    QSqlDatabase db = QSqlDatabase::database(m_SQLManager->currentConnection(), false);
    if (!db.isOpen())
        return;
    if (!m_PTmodel) {
        m_PTmodel = new SqlTreeModel(m_SQLManager->currentConnection(), this);
        m_PTmodel->setRelation(QSqlRelation("PT","ID","PARENT"));
        QMap<int,int> mapping;
        mapping.insert(0,1); mapping.insert(1,2); mapping.insert(2,0);
        m_PTmodel->setColumnMapping(mapping);
        ui->PTtreeView->setModel(m_PTmodel);
        ui->PTtreeView->hideColumn(1);
        ui->PTtreeView->hideColumn(2);
    }
    ui->editPTSqlTableWidget->SetHorizontalHeaders(0,"ID");
    ui->editPTSqlTableWidget->SetHorizontalHeaders(1,"Code Arbre Produit");
    ui->editPTSqlTableWidget->SetHorizontalHeaders(2,"ID du Parent");
    ui->editPTSqlTableWidget->showTable("PT",m_SQLManager->currentConnection());

    m_PTmodel->select();
    m_PTmodel->setHeaderData(0,Qt::Horizontal,"ID");
    m_PTmodel->setHeaderData(1,Qt::Horizontal,"Code Arbre Produit");
    ui->PTtreeView->expandAll();

    ui->editReqSqlTableWidget->SetHorizontalHeaders(1,"Code Requirement");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(2,"Titre du Requirement");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(3,"Description");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(4,"Type");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(5,"Statut");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(6,"Source");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(7,"Racine de traçabilité");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(8,"Commentaires");
    ui->editReqSqlTableWidget->showTable("REQUIREMENT", m_SQLManager->currentConnection());
    ui->editReqSqlTableWidget->addRelationalTable(4,QSqlRelation("REQ_TYPE","ID","TYPE"));
    ui->editReqSqlTableWidget->addRelationalTable(5,QSqlRelation("REQ_STATUS","ID","STATUS"));
    ui->editReqSqlTableWidget->setColumnHidden(0,true);
    //on désactive l'édition classique sur double click
    ui->editReqSqlTableWidget->SetEditionTriggers(QAbstractItemView::NoEditTriggers);


    ui->editreqtypeSqlTableWidget->SetHorizontalHeaders(1,"Type de requirement");
    ui->editreqtypeSqlTableWidget->showTable("REQ_TYPE", m_SQLManager->currentConnection());
    ui->editreqtypeSqlTableWidget->setColumnHidden(0,true);

    ui->editreqmethodSqlTableWidget->SetHorizontalHeaders(1,"Méthode de vérification");
    ui->editreqmethodSqlTableWidget->showTable("REQ_METHOD", m_SQLManager->currentConnection());
    ui->editreqmethodSqlTableWidget->setColumnHidden(0,true);

    ui->editreqstatusSqlTableWidget->SetHorizontalHeaders(1,"Acronyme");
    ui->editreqstatusSqlTableWidget->SetHorizontalHeaders(2,"Statut");
    ui->editreqstatusSqlTableWidget->showTable("REQ_STATUS", m_SQLManager->currentConnection());
    ui->editreqstatusSqlTableWidget->setColumnHidden(0,true);

    ui->editDocumentSqlTableWidget->SetHorizontalHeaders(1,"Code Arbre Produit");
    ui->editDocumentSqlTableWidget->SetHorizontalHeaders(2,"Type de document");
    ui->editDocumentSqlTableWidget->SetHorizontalHeaders(3,"Titre");
    ui->editDocumentSqlTableWidget->SetHorizontalHeaders(4,"Description");
    ui->editDocumentSqlTableWidget->showTable("DOCUMENT", m_SQLManager->currentConnection());
    ui->editDocumentSqlTableWidget->addRelationalTable(1,QSqlRelation("PT","ID","NAME"));
    ui->editDocumentSqlTableWidget->addRelationalTable(2,QSqlRelation("DOC_TYPE","ID","TYPE"));
    ui->editDocumentSqlTableWidget->setColumnHidden(0,true);

    ui->editDocTypeSqlTableWidget->SetHorizontalHeaders(1,"Type de document");
    ui->editDocTypeSqlTableWidget->SetHorizontalHeaders(2,"Description");
    ui->editDocTypeSqlTableWidget->showTable("DOC_TYPE", m_SQLManager->currentConnection());
    ui->editDocTypeSqlTableWidget->setColumnHidden(0,true);

    ui->editReqChapterSqlTableWidget->SetHorizontalHeaders(1,"Document");
    ui->editReqChapterSqlTableWidget->SetHorizontalHeaders(2,"Parent");
    ui->editReqChapterSqlTableWidget->SetHorizontalHeaders(3,"Type de nœud");
    ui->editReqChapterSqlTableWidget->SetHorizontalHeaders(5,"Titre");
    ui->editReqChapterSqlTableWidget->showTable("DOCUMENT_NODE", m_SQLManager->currentConnection());
    ui->editReqChapterSqlTableWidget->setColumnHidden(0,true);

    ui->editConfChangeWidget->SetHorizontalHeaders(1,"Code");
    ui->editConfChangeWidget->SetHorizontalHeaders(2,"Type");
    ui->editConfChangeWidget->SetHorizontalHeaders(3,"Statut");
    ui->editConfChangeWidget->SetHorizontalHeaders(4,"Description");
    ui->editConfChangeWidget->showTable("CHANGE_ITEM",m_SQLManager->currentConnection());
    ui->editConfChangeWidget->addRelationalTable(2,QSqlRelation("CHANGE_TYPE","ID","LABEL"));
    ui->editConfChangeWidget->addRelationalTable(3,QSqlRelation("CHANGE_STATUS","ID","LABEL"));
    ui->editConfChangeWidget->setColumnHidden(0,true);

    ui->editIFWidget->SetHorizontalHeaders(1,"Element n°1");
    ui->editIFWidget->SetHorizontalHeaders(2,"Element n°2");
    ui->editIFWidget->SetHorizontalHeaders(3,"Description");
    ui->editIFWidget->SetHorizontalHeaders(4,"Code");
    ui->editIFWidget->showTable("INTERFACE",m_SQLManager->currentConnection());
    ui->editIFWidget->addRelationalTable(1,QSqlRelation("PT","ID","NAME"));
    ui->editIFWidget->addRelationalTable(2,QSqlRelation("PT","ID","NAME"));
    ui->editIFWidget->setColumnHidden(0,true);

    ui->editIFChaptersWidget->SetHorizontalHeaders(1,"Document");
    ui->editIFChaptersWidget->SetHorizontalHeaders(2,"Parent");
    ui->editIFChaptersWidget->SetHorizontalHeaders(3,"Type de nœud");
    ui->editIFChaptersWidget->SetHorizontalHeaders(5,"Titre");
    ui->editIFChaptersWidget->showTable("DOCUMENT_NODE",m_SQLManager->currentConnection());
    ui->editIFChaptersWidget->setColumnHidden(0,true);

    ui->editModelWidget->SetHorizontalHeaders(1,"Code configuration");
    ui->editModelWidget->SetHorizontalHeaders(2,"Libellé");
    ui->editModelWidget->showTable("CONFIGURATION",m_SQLManager->currentConnection());
    ui->editModelWidget->setColumnHidden(0,true);

}

QVariant MainWindow::getRelatedTablePKvalue(int column,SQLTableForm *tableform, QString searchStr, bool CanBeNull)
{
    //recherche dans la table liée
    QSqlRelation relation = tableform->getRelationAtColumn(column);
    if(!relation.isValid())
        return -1;
    if (!hasOpenProject())
        return QVariant();
    QSqlDatabase db =QSqlDatabase::database(m_SQLManager->currentConnection(), false);
    if(!db.isValid())
        return QVariant();

    if(!db.isOpen())
        return QVariant();

    QSqlQuery query(db);

    query.prepare(QString("SELECT * FROM %1 WHERE %2=?").arg(relation.tableName(),relation.displayColumn()));
    query.addBindValue(searchStr);
    query.exec();

    if(!query.first())
    {
        if(!CanBeNull)
            m_importLog.append(QString("ERREUR: La valeur \"%1\" n'existe pas dans la table liée \"%2\" sur le champ %3.\n").arg(searchStr,relation.tableName(),relation.displayColumn()));
        else
            m_importLog.append(QString("WARNING: La valeur \"%1\" n'existe pas dans la table liée \"%2\" sur le champ %3.\n").arg(searchStr,relation.tableName(),relation.displayColumn()));
    }
    return query.record().value(relation.indexColumn());


}



void MainWindow::on_customTableComboBox_currentIndexChanged(int index)
{
    if (index < 0)
        return;

    if (index == 0)
        ui->stackedWidget->setCurrentIndex(0);
    else
    {
        ui->stackedWidget->setCurrentIndex(1);
        QString queryStr = m_tableViewManager->requestForTableIndex(ui->customTableComboBox->currentIndex() - 1).trimmed();
        ui->customSqlTableWidget->execQuery(queryStr, m_SQLManager->currentConnection());
    }
}

void MainWindow::on_exportSqlToCsvPushButton_clicked()
{
    exportToCsv(ui->manualSqlTableWidget->sqlTableView(),"custom");
}

void MainWindow::on_refreshCustomTablePushButton_clicked()
{
    ui->customSqlTableWidget->execQuery(m_tableViewManager->requestForTableIndex(ui->customTableComboBox->currentIndex() - 1), m_SQLManager->currentConnection());
}

void MainWindow::on_exportCustomViewToCsvPushButton_clicked()
{
    exportToCsv(ui->customSqlTableWidget->sqlTableView(),ui->customTableComboBox->currentText());
}


void MainWindow::exportToCsv(QTableView *sqlTable,QString tableName)
{
    exportCsvDialog* _exportCsvDialog = new exportCsvDialog();
    QStringList headers;
    for(int i = 0; i < sqlTable->model()->columnCount(); i++)
    {
        headers.append(sqlTable->model()->headerData(i, Qt::Horizontal).toString());
    }
    if (_exportCsvDialog->exec(headers,tableName) == QDialog::Accepted)
    {
        QList<int> columnIndexes = _exportCsvDialog->columnIndexes();
        if (columnIndexes.count() > 0)
        {
            QString filename = QFileDialog::getSaveFileName(this,"Enregistrer sous...","","*.csv");
            if (!filename.isEmpty())
            {
                QStringList strList;
                QList<QStringList> rows;
                rows << _exportCsvDialog->columnNames();
                for (int row=0; row < sqlTable->model()->rowCount();++row)
                {
                    strList.clear();
                    for (int column = 0; column < columnIndexes.count();++column)
                        strList <<  sqlTable->model()->index(row,columnIndexes.at(column)).data().toString();
                    rows << strList;
                }
                if (!CsvUtility::write(filename,rows,';'))
                    QMessageBox::warning(this,"Enregistrement en csv","Erreur lors de l'enregistrement du csv...");
            }
        }
    }
}

void MainWindow::on_importTableConfPushButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this,"Tables configuration file",QApplication::applicationDirPath(),"*.ini;*.txt");
    if (!filename.isEmpty())
    {
        m_tableViewManager->loadConfFromFile(filename);
        ui->editTableViewWidget->setTableViewManager(m_tableViewManager);
    }
}

void MainWindow::on_exportTableConfPushButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this,"Tables configuration file",QApplication::applicationDirPath(),"*.ini;*.txt");
    if (!filename.isEmpty())
        m_tableViewManager->saveConfToFile(filename);
}


void MainWindow::on_editTabWidget_currentChanged(int index)
{
    Q_UNUSED(index)
}

void MainWindow::on_exportDocToCsvPushButton_clicked()
{
    exportToCsv(ui->docViewSqlTableWidget->sqlTableView(),"DocumentsLists");
}

void MainWindow::refreshTableSlot()
{
    if (!m_SQLManager)
        return;
    const QString connection = m_SQLManager->currentConnection();
    const QSqlDatabase db = QSqlDatabase::database(connection, false);
    if (connection.isEmpty() || !db.isValid() || !db.isOpen())
        return;
    refreshEditTables();
    refreshViewTables();
    if (m_productTreeWidget)
        m_productTreeWidget->refresh();
    if (m_requirementWidget)
        m_requirementWidget->refresh();
    if (m_traceabilityControlWidget)
        m_traceabilityControlWidget->refresh();
    if (m_documentWidget)
        m_documentWidget->refresh();
    if (m_applicabilityWidget)
        m_applicabilityWidget->refresh();
    if (m_verificationWidget)
        m_verificationWidget->refresh();
    if (m_interfaceWidget)
        m_interfaceWidget->refresh();
    if (m_changeWidget)
        m_changeWidget->refresh();
    if (m_historyWidget)
        m_historyWidget->refresh();
    if (m_referenceDataWidget)
        m_referenceDataWidget->refresh();
    if (m_coverageDashboard)
        m_coverageDashboard->refresh();
    if (m_traceabilityWidget)
        m_traceabilityWidget->refresh();
    if (m_n2MatrixWidget)
        m_n2MatrixWidget->refresh();
}

void MainWindow::setupModernNavigation()
{
    QAction *importReqIf = ui->menuFiles->addAction(tr("Importer ReqIF…"));
    QAction *exportReqIf = ui->menuFiles->addAction(tr("Exporter ReqIF…"));
    connect(importReqIf, &QAction::triggered, this, [this] {
        if (!hasOpenProject()) return;
        const QString path=QFileDialog::getOpenFileName(this,tr("Importer ReqIF"),{},tr("ReqIF (*.reqif *.xml)"));if(path.isEmpty())return;
        ReqIfService service(m_SQLManager->currentConnection());const ReqIfReport report=service.preview(path);
        QString details=report.summary();if(!report.warnings.isEmpty())details+="\n\n"+report.warnings.join('\n');if(!report.errors.isEmpty()){details+="\n\n"+report.errors.join('\n');QMessageBox::critical(this,tr("Rapport ReqIF"),details);return;}
        if(QMessageBox::question(this,tr("Aperçu avant import"),details+tr("\n\nAppliquer cet import transactionnel ?"))!=QMessageBox::Yes)return;
        QString error;if(!service.importFile(path,report,&error)){QMessageBox::critical(this,tr("Import ReqIF"),error);return;}refreshTableSlot();QMessageBox::information(this,tr("Import ReqIF"),tr("Import terminé. ")+report.summary());
    });
    ui->menuFiles->addSeparator();
    QAction *backupProject = ui->menuFiles->addAction(tr("Créer une sauvegarde…"));
    QAction *restoreProject = ui->menuFiles->addAction(tr("Restaurer une sauvegarde…"));
    QAction *diagnoseProject = ui->menuFiles->addAction(tr("Diagnostiquer le projet"));
    connect(backupProject,&QAction::triggered,this,[this]{
        if(!hasOpenProject())return;
        QString suggested=QFileInfo(m_SQLManager->filename()).completeBaseName()+"-backup.db";
        QString file=QFileDialog::getSaveFileName(this,tr("Sauvegarder le projet"),suggested,tr("Projet GeSiER (*.db)"));
        if(file.isEmpty())return;
        if(m_SQLManager->manualBackup(file))QMessageBox::information(this,tr("Sauvegarde"),tr("Sauvegarde créée avec succès."));
        else QMessageBox::warning(this,tr("Sauvegarde"),m_SQLManager->lastError());
    });
    connect(restoreProject,&QAction::triggered,this,[this]{
        if(!hasOpenProject())return;
        QString file=QFileDialog::getOpenFileName(this,tr("Restaurer une sauvegarde"),{},tr("Projet GeSiER (*.db)"));
        if(file.isEmpty())return;
        if(QMessageBox::warning(this,tr("Restaurer"),tr("Le projet courant sera remplacé après création d'une copie de sécurité. Continuer ?"),QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes)return;
        releaseProjectViews();
        if(m_SQLManager->restoreFromBackup(file)){bindProjectViews();QMessageBox::information(this,tr("Restaurer"),tr("Sauvegarde restaurée."));}
        else {bindProjectViews();QMessageBox::warning(this,tr("Restaurer"),m_SQLManager->lastError());}
    });
    connect(diagnoseProject,&QAction::triggered,this,[this]{
        if(!hasOpenProject())return;
        const auto d=HistoryService(m_SQLManager->currentConnection()).diagnose();
        if(d.ok)QMessageBox::information(this,tr("Diagnostic du projet"),d.summary());
        else QMessageBox::warning(this,tr("Diagnostic du projet"),d.summary());
    });
    connect(exportReqIf, &QAction::triggered, this, [this] {
        if (!hasOpenProject()) return;
        QString path=QFileDialog::getSaveFileName(this,tr("Exporter ReqIF"),{},tr("ReqIF (*.reqif)"));
        if (path.isEmpty()) return;
        if (!path.endsWith(".reqif",Qt::CaseInsensitive)) path += ".reqif";
        QString error;
        if (!ReqIfService(m_SQLManager->currentConnection()).exportFile(path,&error))
            QMessageBox::critical(this,tr("Export ReqIF"),error);
        else
            QMessageBox::information(this,tr("Export ReqIF"),tr("Fichier exporté."));
    });
    QWidget *dashboardPage=new QWidget(this);
    QVBoxLayout *dashboardLayout=new QVBoxLayout(dashboardPage);
    dashboardLayout->setContentsMargins(0,0,0,0);
    m_coverageDashboard->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Expanding);
    dashboardLayout->addWidget(m_coverageDashboard,1);

    QWidget *projectPage=new QWidget(this);QHBoxLayout *projectLayout=new QHBoxLayout(projectPage);
    m_projectNavigation=new QListWidget(projectPage);m_projectNavigation->setMaximumWidth(210);
    m_projectPages=new QStackedWidget(projectPage);
    m_productTreeWidget=new ProductTreeWidget(projectPage);
    m_requirementWidget=new RequirementWidget(projectPage);
    m_traceabilityControlWidget=new TraceabilityControlWidget(projectPage);
    m_documentWidget=new DocumentWidget(projectPage);
    m_applicabilityWidget=new ApplicabilityWidget(projectPage);
    m_verificationWidget=new VerificationWidget(projectPage);
    m_interfaceWidget=new InterfaceWidget(projectPage);
    m_changeWidget=new ChangeWidget(projectPage);
    m_historyWidget=new HistoryWidget(projectPage);
    m_referenceDataWidget=new ReferenceDataWidget(projectPage);
    const QStringList sections={tr("Product Tree"),tr("Exigences"),tr("Traçabilité"),tr("Documents"),tr("Applicabilité"),tr("Interfaces"),tr("Vérification"),tr("Changements"),tr("Historique"),tr("Paramètres")};
    m_projectNavigation->addItems(sections);
    m_projectPages->addWidget(m_productTreeWidget);
    m_projectPages->addWidget(m_requirementWidget);
    m_projectPages->addWidget(m_traceabilityControlWidget);
    m_projectPages->addWidget(m_documentWidget);
    m_projectPages->addWidget(m_applicabilityWidget);
    m_projectPages->addWidget(m_interfaceWidget);
    m_projectPages->addWidget(m_verificationWidget);
    m_projectPages->addWidget(m_changeWidget);
    m_projectPages->addWidget(m_historyWidget);
    m_projectPages->addWidget(m_referenceDataWidget);
    projectLayout->addWidget(m_projectNavigation);projectLayout->addWidget(m_projectPages,1);
    connect(m_projectNavigation,&QListWidget::currentRowChanged,m_projectPages,&QStackedWidget::setCurrentIndex);
    connect(m_productTreeWidget,&ProductTreeWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_requirementWidget,&RequirementWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_traceabilityControlWidget,&TraceabilityControlWidget::openRequirementRequested,this,[this](int id){m_requirementWidget->openRequirement(id);m_projectNavigation->setCurrentRow(RequirementsPage);});
    connect(m_documentWidget,&DocumentWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_applicabilityWidget,&ApplicabilityWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_applicabilityWidget,&ApplicabilityWidget::openRequirementRequested,this,[this](int id){ui->mainTabWidget->setCurrentIndex(1);m_projectNavigation->setCurrentRow(RequirementsPage);m_requirementWidget->openRequirementApplicability(id);});
    connect(m_verificationWidget,&VerificationWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_verificationWidget,&VerificationWidget::openRequirementRequested,this,[this](int id){ui->mainTabWidget->setCurrentIndex(1);m_projectNavigation->setCurrentRow(RequirementsPage);m_requirementWidget->openRequirement(id);});
    connect(m_documentWidget,&DocumentWidget::openRequirement,this,[this](int id){ui->mainTabWidget->setCurrentIndex(1);m_projectNavigation->setCurrentRow(RequirementsPage);m_requirementWidget->openRequirement(id);});
    connect(m_requirementWidget,&RequirementWidget::openDocumentRequested,this,[this](int id){ui->mainTabWidget->setCurrentIndex(1);m_projectNavigation->setCurrentRow(DocumentsPage);m_documentWidget->openDocument(id);});
    connect(m_productTreeWidget,&ProductTreeWidget::openRequirementsForPt,this,[this](int pt){RequirementFilter f;f.ptIds={pt};m_requirementWidget->applyFilter(f);ui->mainTabWidget->setCurrentIndex(1);m_projectNavigation->setCurrentRow(RequirementsPage);});
    connect(m_productTreeWidget,&ProductTreeWidget::openInterfacesForPt,this,[this](int pt){m_interfaceWidget->applyPtFilter(pt);ui->mainTabWidget->setCurrentIndex(1);m_projectNavigation->setCurrentRow(InterfacesPage);});
    connect(m_interfaceWidget,&InterfaceWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_interfaceWidget,&InterfaceWidget::openRequirementRequested,this,[this](int id){m_requirementWidget->openRequirement(id);m_projectNavigation->setCurrentRow(RequirementsPage);});
    connect(m_interfaceWidget,&InterfaceWidget::openDocumentRequested,this,[this](int id){m_documentWidget->openDocument(id);m_projectNavigation->setCurrentRow(DocumentsPage);});
    connect(m_interfaceWidget,&InterfaceWidget::openProductTreeRequested,this,[this](int){m_projectNavigation->setCurrentRow(ProductTreePage);});
    connect(m_productTreeWidget,&ProductTreeWidget::openChangesForPt,this,[this](int id){m_changeWidget->applyObjectFilter("PT",id);ui->mainTabWidget->setCurrentIndex(1);m_projectNavigation->setCurrentRow(ChangesPage);});
    connect(m_requirementWidget,&RequirementWidget::openChangesRequested,this,[this](int id){m_changeWidget->applyObjectFilter("REQUIREMENT",id);m_projectNavigation->setCurrentRow(ChangesPage);});
    connect(m_requirementWidget,&RequirementWidget::openChangeRequested,this,[this](int id){m_changeWidget->openChange(id);m_projectNavigation->setCurrentRow(ChangesPage);});
    connect(m_changeWidget,&ChangeWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_referenceDataWidget,&ReferenceDataWidget::dataChanged,this,&MainWindow::refreshTableSlot);
    connect(m_changeWidget,&ChangeWidget::openRequirementRequested,this,[this](int id){m_requirementWidget->openRequirement(id);m_projectNavigation->setCurrentRow(RequirementsPage);});
    connect(m_changeWidget,&ChangeWidget::openProductTreeRequested,this,[this](int){m_projectNavigation->setCurrentRow(ProductTreePage);});
    connect(m_changeWidget,&ChangeWidget::openInterfaceRequested,this,[this](int id){m_interfaceWidget->openInterface(id);m_projectNavigation->setCurrentRow(InterfacesPage);});
    connect(m_changeWidget,&ChangeWidget::openDocumentRequested,this,[this](int id){m_documentWidget->openDocument(id);m_projectNavigation->setCurrentRow(DocumentsPage);});
    connect(m_changeWidget,&ChangeWidget::openConfigurationRequested,this,[this](int){m_projectNavigation->setCurrentRow(ApplicabilityPage);});
    connect(m_historyWidget,&HistoryWidget::openObjectRequested,this,[this](const QString&type,int id){
        if(type=="REQUIREMENT"){m_requirementWidget->openRequirement(id);m_projectNavigation->setCurrentRow(RequirementsPage);}
        else if(type=="DOCUMENT"){m_documentWidget->openDocument(id);m_projectNavigation->setCurrentRow(DocumentsPage);}
        else if(type=="INTERFACE"){m_interfaceWidget->openInterface(id);m_projectNavigation->setCurrentRow(InterfacesPage);}
        else if(type=="CHANGE"){m_changeWidget->openChange(id);m_projectNavigation->setCurrentRow(ChangesPage);}
        else if(type=="CONFIGURATION")m_projectNavigation->setCurrentRow(ApplicabilityPage);
        else if(type=="PT")m_projectNavigation->setCurrentRow(ProductTreePage);
    });
    connect(m_coverageDashboard, &CoverageDashboard::navigateRequested, this,
            [this](int page, const QString &filter) {
      if (page == RequirementsPage) {
        RequirementFilter f;
        if (filter == "unallocated") f.allocated = 0;
        else if (filter == "allocated") f.allocated = 1;
        else if (filter == "untraced") f.traced = 0;
        else if (filter == "traced") f.traced = 1;
        else if (filter == "undocumented") f.documented = 0;
        else if (filter == "documented") f.documented = 1;
        else if (filter == "unverified") f.verified = 0;
        else if (filter == "verified") f.verified = 1;
        m_requirementWidget->applyFilter(f);
      } else if (page == VerificationPage) {
        m_verificationWidget->applyCoverageFilter(filter);
      }
      ui->mainTabWidget->setCurrentIndex(1);
      m_projectNavigation->setCurrentRow(page);
    });
    m_projectNavigation->setCurrentRow(ProductTreePage);
    if (hasOpenProject())
        bindProjectViews();

    ui->mainTabWidget->clear();
    ui->mainTabWidget->addTab(dashboardPage,tr("Tableau de bord"));
    ui->mainTabWidget->addTab(projectPage,tr("Projet"));
    updateProjectUi();
}

void MainWindow::on_exportReqListToCsvPushButton_clicked()
{
    exportToCsv(ui->ReqViewSqlTableWidget->sqlTableView(),"RequirementLists");
}

void MainWindow::on_importTablePushButton_clicked()
{

    QString current_tab=ui->editTabWidget->currentWidget()->objectName();
    if(current_tab=="Tab_PT_edit")
    {
        //import classique
//        importBasic(ui->editTabWidget->currentWidget()->findChild<SQLTableForm*>("editPTSqlTableWidget"));
        importBasic(ui->editPTSqlTableWidget);
    }
    else if(current_tab=="tab_req_edit")
    {
        importReq();
    }
    else if(current_tab=="tab_reqinput_edit")
    {
        //3 tables à gérer
    }
    else if(current_tab=="Tab_Document_edit")
    {
        //2 tables à gérer? Type de doc peut être supprimée je pense...
    }


}



void MainWindow::importReq()
{
    m_importLog.clear();

    if (!hasOpenProject())
        return;
    RequirementImportDialog normalizedImport(m_SQLManager->currentConnection(), this);
    if (normalizedImport.exec() == QDialog::Accepted)
        refreshViewTables();
    return;

#if 0 // Ancien import positionnel, conservé uniquement pour référence de migration.
    QSqlDatabase db = QSqlDatabase::database(m_SQLManager->currentConnection(), false);

    if(!db.isValid())
        return;

    if(!db.isOpen())
        return;

    //vérification qu'il y a au moins une entrée dans les tables étrangères pour les entrées ne pouvant être nulles.
    QStringList currentdbTables = db.tables();

    if(!currentdbTables.contains("PT"))
    {
        QMessageBox::critical(this,"Erreur de base de donnée","La table \"PT\" est absente de la base de donnée");
        return;
    }

    if(!currentdbTables.contains("DOCUMENT"))
    {
        QMessageBox::critical(this,"Erreur de base de donnée","La table \"DOCUMENT\" est absente de la base de donnée");
        return;
    }

    if(!currentdbTables.contains("REQ_TYPE"))
    {
        QMessageBox::critical(this,"Erreur de base de donnée","La table \"REQ_TYPE\" est absente de la base de donnée");
        return;
    }

    if(!currentdbTables.contains("REQ_STATUS"))
    {
        QMessageBox::critical(this,"Erreur de base de donnée","La table \"REQ_STATUS\" est absente de la base de donnée");
        return;
    }

    if(!currentdbTables.contains("REQ_METHOD"))
    {
        QMessageBox::critical(this,"Erreur de base de donnée","La table \"REQ_METHOD\" est absente de la base de donnée");
        return;
    }

    QSqlQuery query(db);

    query.exec("SELECT COUNT(*) FROM PT");
    query.first();
    if(query.value(0).toInt()<1)
    {
        QMessageBox::warning(this,"Erreur de base de donnée","La table \"PT\" ne contient pas d'enregistrement");
        return;
    }


    query.exec("SELECT COUNT(*) FROM DOCUMENT");
    query.first();
    if(query.value(0).toInt()<1)
    {
        QMessageBox::warning(this,"Erreur de base de donnée","La table \"DOCUMENT\" ne contient pas d'enregistrement");
        return;
    }


    query.exec("SELECT COUNT(*) FROM REQ_TYPE");
    query.first();
    if(query.value(0).toInt()<1)
    {
        QMessageBox::warning(this,"Erreur de base de donnée","La table \"REQ_TYPE\" ne contient pas d'enregistrement");
        return;
    }


    query.exec("SELECT COUNT(*) FROM REQ_STATUS");
    query.first();
    if(query.value(0).toInt()<1)
    {
        QMessageBox::warning(this,"Erreur de base de donnée","La table \"REQ_STATUS\" ne contient pas d'enregistrement");
        return;
    }


    query.exec("SELECT COUNT(*) FROM REQ_METHOD");
    query.first();
    if(query.value(0).toInt()<1)
    {
        QMessageBox::warning(this,"Erreur de base de donnée","La table \"REQ_METHOD\" ne contient pas d'enregistrement");
        return;
    }


//    query.finish();

    //récupération des headers de la base de donnée (utile si aliasée par une relation)
//    QStringList tableFieldNames=ui->editReqSqlTableWidget->getFieldNames();

    //ouverture du csv
    QString filename = QFileDialog::getOpenFileName(this,"Importation","","*.csv");

    if (filename.isEmpty())
        return;

    QList<QStringList> data = CsvUtility::read(filename, ';');
    if (data.count()<2)//ne permet pas de changer le séparateur...
        data = CsvUtility::read(filename, ',');
    if (data.count()<2)
    {
        QMessageBox::warning(this,"Fichier CSV","Le fichier CSV est vide ou le séparateur n'est pas \",\" ou \";\" ....");
        return;
    }

    ImportRequirementDialog *importDialog = new ImportRequirementDialog(data.first(),this);

    if(!importDialog->exec())
        return;

    //association des champs
    QMap<int,int> import_map =importDialog->getCSVMapping();
    QMap<int,int>::iterator it_import;
    for(int i=1;i<data.count();i++)
    {
        QMap<int, QVariant> fieldsInput;
        for(it_import=import_map.begin();it_import!=import_map.end();it_import++)
        {
            //Si on est sur le product tree, on cherche ce qui lui est lié
            if(it_import.key()==1)
            {
                //Si on le déduit du requirement
                QString PTString;
                if(importDialog->isPTfromReqCodeChecked())
                {
                    PTString = data.at(i).at(import_map[2]);
                    QStringList PTSplit = PTString.split(importDialog->getSeparateur());
                    PTString.clear();
                    for(int nb_str=0;nb_str<PTSplit.count()-1;nb_str++)
                    {
                        PTString+=PTSplit.at(nb_str)+importDialog->getSeparateur();
                    }
                    PTString.resize(PTString.size()-importDialog->getSeparateur().size());
                }
                else
                {
                    PTString = data.at(i).at(it_import.value());
                }

                fieldsInput.insert(it_import.key(),getRelatedTablePKvalue(1,ui->editReqSqlTableWidget,PTString));

                continue;
            }
            else if(it_import.key()==3) //Document
            {
                fieldsInput.insert(it_import.key(),getRelatedTablePKvalue(3,ui->editReqSqlTableWidget,data.at(i).at(it_import.value())));
                continue;
            }
            else if(it_import.key()==4) //Chapitre
            {
                //Cas où on crée le chapitre ne pas le gérer ici
//                if(importDialog->isChapGenChecked())
//                {
//                    QMap<int,QVariant> chapterFields;
//                    chapterFields.insert(1,fieldsInput[3]);
//                    chapterFields.insert(2,data.at(i).at(it_import.value()));
//                    ui->editReqChapterSqlTableWidget->insertRow(chapterFields);

//                }
                fieldsInput.insert(it_import.key(),getRelatedTablePKvalue(4,ui->editReqSqlTableWidget,data.at(i).at(it_import.value()),true));
                continue;
            }
            else if(it_import.key()==7) //Type
            {
                fieldsInput.insert(it_import.key(),getRelatedTablePKvalue(7,ui->editReqSqlTableWidget,data.at(i).at(it_import.value())));
                continue;
            }
            else if(it_import.key()==8) //Status
            {
                fieldsInput.insert(it_import.key(),getRelatedTablePKvalue(8,ui->editReqSqlTableWidget,data.at(i).at(it_import.value())));
                continue;
            }
            else if(it_import.key()==10) //Parent
            {
                fieldsInput.insert(it_import.key(),getRelatedTablePKvalue(10,ui->editReqSqlTableWidget,data.at(i).at(it_import.value()),true));
                continue;
            }
            else if(it_import.key()==12) //Method
            {
                fieldsInput.insert(it_import.key(),getRelatedTablePKvalue(12,ui->editReqSqlTableWidget,data.at(i).at(it_import.value())));
                continue;
            }
            fieldsInput.insert(it_import.key(),data.at(i).at(it_import.value()));
        }

        //edition ou non?
        int editrow=ui->editReqSqlTableWidget->findRow(ui->editReqSqlTableWidget->getFieldIndex("CODE"),data.at(i).at(import_map[2]));
        //            qDebug()<<__FUNCTION__<<"code requirement: "<<data.at(i).at(import_map["CODE"])<<" "<<"index: "<<i;
        if(editrow>=0)
        {
            ui->editReqSqlTableWidget->updateRow(editrow,fieldsInput);
        }
        else
        {
            ui->editReqSqlTableWidget->insertRow(fieldsInput);
        }
    }


    delete importDialog;

    if(!m_importLog.isEmpty())
    {
        ImportLogDialog *logDialog = new ImportLogDialog(m_importLog);
        logDialog->exec();
        delete logDialog;
//        QMessageBox::warning(this,"Erreur Log",QString("Les erreurs suivantes ont eu lieu lors de l'import:\n%1").arg(m_importLog));
    }
#endif
}

void MainWindow::importBasic(SQLTableForm *tableform)
{
    m_importLog.clear();

    if (!hasOpenProject())
        return;
    QSqlDatabase db = QSqlDatabase::database(m_SQLManager->currentConnection(), false);

    if(!db.isValid())
        return;

    if(!db.isOpen())
        return;

    //ouverture du csv
    QString filename = QFileDialog::getOpenFileName(this,"Importation","","*.csv");

    if (filename.isEmpty())
        return;

    QList<QStringList> data = CsvUtility::read(filename, ';');
    if (data.isEmpty())
        data = CsvUtility::read(filename, ',');
    if (data.isEmpty())
    {
        QMessageBox::warning(this,"Fichier CSV","Le fichier CSV est vide ou le séparateur n'est pas \",\" ou \";\" ....");
        return;
    }

    ImportBasicDialog *importDialog = new ImportBasicDialog(tableform->tableName(),tableform->getHorizontalHeadersNames(),data.first(),this);

    if(!importDialog->exec())
        return;

    //association des champs
    QMap<int,int> import_map =importDialog->getCSVMapping();
    QMap<int,int>::iterator it_import;
    for(int i=1;i<data.count();i++)
    {
        QMap<int, QVariant> fieldsInput;
        for(it_import=import_map.begin();it_import!=import_map.end();it_import++)
        {
            fieldsInput.insert(it_import.key(),data.at(i).at(it_import.value()));
        }

        //edition ou non?
        int PK_index=tableform->getFieldIndex(tableform->primaryKey().fieldName(0));
        int editrow=tableform->findRow(PK_index,data.at(i).at(import_map[PK_index]));

        if(editrow>=0)
        {
            tableform->updateRow(editrow,fieldsInput);
        }
        else
        {
            tableform->insertRow(fieldsInput);
        }
    }


    delete importDialog;

    if(!m_importLog.isEmpty())
    {
        ImportLogDialog *logDialog = new ImportLogDialog(m_importLog);
        logDialog->exec();
        delete logDialog;
    }


}

void MainWindow::importChapter(SQLTableForm *tableform)
{
    m_importLog.clear();

    if (!hasOpenProject())
        return;
    QSqlDatabase db = QSqlDatabase::database(m_SQLManager->currentConnection(), false);

    if(!db.isValid())
        return;

    if(!db.isOpen())
        return;

    //ouverture du csv
    QString filename = QFileDialog::getOpenFileName(this,"Importation","","*.csv");

    if (filename.isEmpty())
        return;

    QList<QStringList> data = CsvUtility::read(filename, ';');
    if (data.isEmpty())
        data = CsvUtility::read(filename, ',');
    if (data.isEmpty())
    {
        QMessageBox::warning(this,"Fichier CSV","Le fichier CSV est vide ou le séparateur n'est pas \",\" ou \";\" ....");
        return;
    }

    ImportBasicDialog *importDialog = new ImportBasicDialog(tableform->tableName(),tableform->getHorizontalHeadersNames(),data.first(),this);

    if(!importDialog->exec())
        return;

    //Chapitres déjà en BD
    QSqlQuery query(db);
    query.exec("SELECT DOC_ID,TITLE FROM DOCUMENT_NODE WHERE NODE_TYPE='CHAPTER'");
    QMap<int,QStringList> chapters;
    int ID;
    if(query.first())
    {
        //ajout des éléments
        ID=query.record().value(0).toInt();
        chapters.insert(ID,chapters[ID]<<query.record().value(1).toString());

        while(query.next())
        {
            //ajout des éléments
            ID=query.record().value(0).toInt();
            chapters.insert(ID,chapters[ID]<<query.record().value(1).toString());
        }
    }

    //association des champs
    QMap<int,int> import_map =importDialog->getCSVMapping();
    QMap<int,int>::iterator it_import;
    for(int i=1;i<data.count();i++)
    {
        QMap<int, QVariant> fieldsInput;
        for(it_import=import_map.begin();it_import!=import_map.end();it_import++)
        {
            if(it_import.key()==0)//0 car il s'agit de la colonne de la vue (=DOC name)
                fieldsInput.insert(1,getRelatedTablePKvalue(1,tableform,data.at(i).at(it_import.value()))); //1 en input car on est sur le modèle (=DOC name)
            else
                fieldsInput.insert(2,data.at(i).at(it_import.value()));//2 en input car on est sur le modèle (=CHAPTER)

        }

        //ajout ou non?
        if(chapters.contains(fieldsInput[1].toInt()))
        {
            if(!chapters[fieldsInput[1].toInt()].contains(fieldsInput[2].toString()))
            {
                tableform->insertRow(fieldsInput);
                chapters.insert(fieldsInput[1].toInt(),chapters[fieldsInput[1].toInt()]<<fieldsInput[2].toString());
            }
        }
        else
        {
            tableform->insertRow(fieldsInput);
            chapters.insert(fieldsInput[1].toInt(),QStringList(fieldsInput[2].toString()));
        }
    }


    delete importDialog;

    if(!m_importLog.isEmpty())
    {
        ImportLogDialog *logDialog = new ImportLogDialog(m_importLog);
        logDialog->exec();
        delete logDialog;
    }
}
