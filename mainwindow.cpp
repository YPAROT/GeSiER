#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QSettings>
#include <QSqlError>
#include <QMessageBox>
#include <QtSql>
#include <qwidgetsettings.h>
#include <QStandardItemModel>
#include "exportcsvdialog.h"
#include "importrequirementdialog.h"
#include "importbasicdialog.h"
#include "importlogdialog.h"

#include "qtcsv/reader.h"
#include "qtcsv/writer.h"
#include "qtcsv/stringdata.h"
#include "qtcsv/variantdata.h"

#include <QTableWidget> //ajout après suppression etiquette


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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


    QString filename = QWidgetSettings::value("REQ").toString();
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


            refreshEditTables();

            //connection des commits au refresh
            connect(ui->editPTSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editReqSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editreqmethodSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editreqstatusSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editreqtypeSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editDocumentSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editDocTypeSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editReqChapterSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editConfChangeWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editIFWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editIFChaptersWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editModelWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));
            connect(ui->editPartsWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));


            refreshViewTables();
        }
    }

    //connection des commits au refresh
    connect(ui->editPTSqlTableWidget,SIGNAL(comitOccured()),this,SLOT(refreshTableSlot()));

    emit statusMessage(tr("Ready."));
}

MainWindow::~MainWindow()
{
    m_SQLManager->close();
    delete m_SQLManager;
    m_tableViewManager->saveConfToFile(QApplication::applicationDirPath()+"\\defaultConf.ini");
    delete m_tableViewManager;
    delete m_PTmodel;
    delete ui;
}

void MainWindow::on_actionClose_triggered()
{
    m_SQLManager->close();
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
            QSqlError err = m_SQLManager->newDB(filename);
            if (err.type() != QSqlError::NoError)
                QMessageBox::warning(this, tr("Unable to create database"), tr("An error occurred while "
                                                                               "creating the connection: ") + err.text());
            else
            {
                QWidgetSettings::setValue("REQ",filename);
                setWindowTitle("GEstion  SImplifée d'Exigences pour La Recherche: "+filename);
            }
        }
    }

    refreshEditTables();
    refreshViewTables();
}

void MainWindow::on_actionLoad_DB_triggered()
{
    QString filename = QFileDialog::getOpenFileName(this,"Ouvrir une base de données",QApplication::applicationDirPath(),"*.db");
    if (!filename.isEmpty())
    {
        QSqlError err = m_SQLManager->openDB(filename);
        if (err.type() != QSqlError::NoError)
            QMessageBox::warning(this, tr("Unable to open database"), tr("An error occurred while "
                                                                         "opening the connection: ") + err.text());
        else
        {
            QWidgetSettings::setValue("REQ",filename);
            setWindowTitle("GEstion  SImplifée d'Exigences pour La Recherche: "+filename);
        }
    }
    refreshEditTables();
    ui->ReqTrackViewWidget->init();
    refreshViewTables();
}

void MainWindow::on_actionSave_as_triggered()
{
    QString filename = QFileDialog::getSaveFileName(this,"Enregistrer sous...","","*.db");
    if (!filename.isEmpty())
    {
        if (m_SQLManager->saveAs(filename) == false)
            QMessageBox::warning(this,"Enregistrer sous...","Erreur lors du renommage du fichier");
        else
        {
            QWidgetSettings::setValue("REQ",filename);
            setWindowTitle("GEstion  SImplifée d'Exigences pour La Recherche: "+filename);
        }
    }
}

void MainWindow::refreshViewTables()
{
      ui->docViewSqlTableWidget->execQuery("SELECT D.TITLE AS [Titre],D.DESCRIPTION AS [Description],REQLIST.REQ_COUNT [Nombre de Requirements],CC.CH_COUNT AS [Nombre de Chapitres] "
                                           "FROM (SELECT R.DOC_ID,COUNT(*) AS [REQ_COUNT]FROM REQUIREMENT R GROUP BY R.DOC_ID) REQLIST "
                                           "LEFT JOIN DOCUMENT D ON REQLIST.DOC_ID=D.ID "
                                           "LEFT JOIN (SELECT RC.DOC_ID,COUNT(*) AS CH_COUNT FROM REQ_CHAPTER RC GROUP BY RC.DOC_ID) CC ON REQLIST.DOC_ID=CC.DOC_ID",m_SQLManager->currentConnection());

      ui->docIFViewSqlTableWidget->execQuery("SELECT D.TITLE AS [Titre],D.DESCRIPTION AS [Description],IFLIST.IF_COUNT [Nombre d'Interfaces],CC.CH_COUNT AS [Nombre de Chapitres] "
                                           "FROM (SELECT IF.DOC_ID,COUNT(*) AS [IF_COUNT]FROM INTERFACE IF GROUP BY IF.DOC_ID) IFLIST "
                                           "LEFT JOIN DOCUMENT D ON IFLIST.DOC_ID=D.ID "
                                           "LEFT JOIN (SELECT IFC.DOC_ID,COUNT(*) AS CH_COUNT FROM IF_CHAPTER IFC) CC ON IFLIST.DOC_ID=CC.DOC_ID",m_SQLManager->currentConnection());

      ui->ReqViewSqlTableWidget->execQuery("SELECT R.CODE AS [Req. Code],R.TITLE AS [Titre],PT.NAME AS [Element du PT],D.TITLE AS [Document associé], RC.CHAPTER AS [Chapitre], R.DESCRIPTION AS [Description], "
                                           "RT.TYPE AS [Type], RS.STATUS AS [Statut], R.SOURCE AS [Source], RP.CODE AS [Req. parent], R.VERIF_LEVEL AS [Niveau de vérification], RM.METHOD AS [Méthode de vérification], R.COMMENTS AS [Commentaires]"
                                           "FROM REQUIREMENT R "
                                           "LEFT JOIN PT ON R.PT_ID=PT.ID "
                                           "LEFT JOIN DOCUMENT AS D ON R.DOC_ID=D.ID "
                                           "LEFT JOIN REQ_CHAPTER AS RC ON R.DOC_CHAPTER=RC.ID "
                                           "LEFT JOIN REQ_TYPE AS RT ON R.TYPE=RT.ID "
                                           "LEFT JOIN REQ_STATUS AS RS ON R.STATUS=RS.ID "
                                           "LEFT JOIN REQUIREMENT AS RP ON R.PARENT_ID=RP.ID "
                                           "LEFT JOIN REQ_METHOD AS RM ON R.VERIF_METHOD=RM.ID ",m_SQLManager->currentConnection());

      ui->ReqTrackViewWidget->refresh();

      ui->ConfChangeWidget->execQuery("SELECT R.CODE AS [Req. Code], CC.DESCRIPTION AS [Description], CCS.STATUS AS [Statut] "
                                      "FROM CONF_CHANGE CC "
                                      "LEFT JOIN REQUIREMENT R ON CC.REQ_ID=R.ID "
                                      "LEFT JOIN CC_STATUS CCS ON CC.STATUS=CCS.ID",m_SQLManager->currentConnection());

      ui->IFWidget->execQuery("SELECT PT1.NAME AS [Element 1], PT2.NAME AS [Element 2], D.TITLE AS [Document associé], IF.DESCRIPTION AS [Description], DC.CHAPTER AS [Chapitre] "
                              "FROM INTERFACE IF "
                              "LEFT JOIN PT AS PT1 ON IF.ELEMENT1=PT1.ID "
                              "LEFT JOIN PT AS PT2 ON IF.ELEMENT2=PT2.ID "
                              "LEFT JOIN DOCUMENT D ON IF.DOC_ID=D.ID "
                              "LEFT JOIN IF_CHAPTER DC ON IF.DOC_CHAPTER=DC.ID",m_SQLManager->currentConnection());

      ui->ModelWidget->execQuery("SELECT MODEL AS [Modèle Instrument] FROM MODEL",m_SQLManager->currentConnection());

      ui->PartsWidget->execQuery("SELECT PT.NAME AS [Element du PT], P.NAME AS [Identifiant du composant], P.SERIAL AS [Numéro de série], M.MODEL AS [Modèle Instrument associé], P.DESCRIPTION AS [Description] "
                                 "FROM PART P "
                                 "LEFT JOIN PT ON P.PT_ID=PT.ID "
                                 "LEFT JOIN MODEL M ON P.MODEL=M.ID",m_SQLManager->currentConnection());

      ui->LogsWidget->execQuery("SELECT RL.LOG_TIME AS[Date de Changement],RL.USER_ACTION AS [Type de Changement],R.CODE AS [Code Requirement], "
                                "CASE RL.OLD_DOC_ID WHEN RL.NEW_DOC_ID THEN \"No Change\" ELSE \"WAS: \"||ifnull(D_O.TITLE, \"NULL\")||char(10)||\"IS: \"||ifnull(D_N.TITLE,\"NULL\") END AS [Changement de Document], "
                                "CASE RL.OLD_DOC_CHAPTER WHEN RL.NEW_DOC_CHAPTER THEN \"No Change\" ELSE \"WAS: \"||ifnull(RC_O.CHAPTER, \"NULL\")||char(10)||\"IS: \"||ifnull(RC_N.CHAPTER,\"NULL\") END AS [Changement de Chapitre], "
                                "CASE RL.OLD_TITLE WHEN RL.NEW_TITLE THEN \"No Change\" ELSE \"WAS: \"||ifnull(RL.OLD_TITLE, \"NULL\")||char(10)||\"IS: \"||ifnull(RL.NEW_TITLE,\"NULL\") END AS [Changement de Titre], "
                                "CASE RL.OLD_DESCRIPTION WHEN RL.NEW_DESCRIPTION THEN \"No Change\" ELSE \"WAS: \"||ifnull(RL.OLD_DESCRIPTION, \"NULL\")||char(10)||\"IS: \"||ifnull(RL.NEW_DESCRIPTION,\"NULL\") END AS [Changement de Description], "
                                "CASE RL.OLD_TYPE WHEN RL.NEW_TYPE THEN \"No Change\" ELSE \"WAS: \"||ifnull(RT_O.TYPE, \"NULL\")||char(10)||\"IS: \"||ifnull(RT_N.TYPE,\"NULL\") END AS [Changement de Type], "
                                "CASE RL.OLD_STATUS WHEN RL.NEW_STATUS THEN \"No Change\" ELSE \"WAS: \"||ifnull(RS_O.STATUS, \"NULL\")||char(10)||\"IS: \"||ifnull(RS_N.STATUS,\"NULL\") END AS [Changement de Statut], "
                                "CASE RL.OLD_SOURCE WHEN RL.NEW_SOURCE THEN \"No Change\" ELSE \"WAS: \"||ifnull(RL.OLD_SOURCE, \"NULL\")||char(10)||\"IS: \"||ifnull(RL.NEW_SOURCE,\"NULL\") END AS [Changement de Source], "
                                "CASE RL.OLD_PARENT_ID WHEN RL.NEW_PARENT_ID THEN \"No Change\" ELSE \"WAS: \"||ifnull(RP_O.CODE, \"NULL\")||char(10)||\"IS: \"||ifnull(RP_N.CODE,\"NULL\") END AS [Changement de Parent], "
                                "CASE RL.OLD_VERIF_LEVEL WHEN RL.NEW_VERIF_LEVEL THEN \"No Change\" ELSE \"WAS: \"||ifnull(RL.OLD_VERIF_LEVEL, \"NULL\")||char(10)||\"IS: \"||ifnull(RL.NEW_VERIF_LEVEL,\"NULL\") END AS [Changement de Niveau de Vérification], "
                                "CASE RL.OLD_VERIF_METHOD WHEN RL.NEW_VERIF_METHOD THEN \"No Change\" ELSE \"WAS: \"||ifnull(RM_O.METHOD, \"NULL\")||char(10)||\"IS: \"||ifnull(RM_N.METHOD,\"NULL\") END AS [Changement de Méthode de Vérification], "
                                "CASE RL.OLD_COMMENTS WHEN RL.NEW_COMMENTS THEN \"No Change\" ELSE \"WAS: \"||ifnull(RL.OLD_COMMENTS, \"NULL\")||char(10)||\"IS: \"||ifnull(RL.NEW_COMMENTS,\"NULL\") END AS [Changement de Commentaire] "
                                "FROM REQ_LOGS RL "
                                "LEFT JOIN REQUIREMENT R ON RL.REQ_ID=R.ID "
                                "LEFT JOIN REQ_CHAPTER RC_O ON RL.OLD_DOC_CHAPTER=RC_O.ID "
                                "LEFT JOIN REQ_CHAPTER RC_N ON RL.NEW_DOC_CHAPTER=RC_N.ID "
                                "LEFT JOIN DOCUMENT D_O ON RL.OLD_DOC_ID=D_O.ID "
                                "LEFT JOIN DOCUMENT D_N ON RL.NEW_DOC_ID=D_N.ID "
                                "LEFT JOIN REQ_TYPE RT_O ON RL.OLD_TYPE=RT_O.ID "
                                "LEFT JOIN REQ_TYPE RT_N ON RL.NEW_TYPE=RT_N.ID "
                                "LEFT JOIN REQ_STATUS RS_O ON RL.OLD_STATUS=RS_O.ID "
                                "LEFT JOIN REQ_STATUS RS_N ON RL.NEW_STATUS=RS_N.ID "
                                "LEFT JOIN REQUIREMENT RP_O ON RL.OLD_PARENT_ID=RP_O.ID "
                                "LEFT JOIN REQUIREMENT RP_N ON RL.NEW_PARENT_ID=RP_N.ID "
                                "LEFT JOIN REQ_METHOD RM_O ON RL.OLD_VERIF_METHOD=RM_O.ID "
                                "LEFT JOIN REQ_METHOD RM_N ON RL.NEW_VERIF_METHOD=RM_N.ID",m_SQLManager->currentConnection());


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
    ui->editPTSqlTableWidget->SetHorizontalHeaders(0,"ID");
    ui->editPTSqlTableWidget->SetHorizontalHeaders(1,"Code Arbre Produit");
    ui->editPTSqlTableWidget->SetHorizontalHeaders(2,"ID du Parent");
    ui->editPTSqlTableWidget->showTable("PT",m_SQLManager->currentConnection());

    m_PTmodel->select();
    m_PTmodel->setHeaderData(0,Qt::Horizontal,"ID");
    m_PTmodel->setHeaderData(1,Qt::Horizontal,"Code Arbre Produit");
    ui->PTtreeView->expandAll();

    ui->editReqSqlTableWidget->SetHorizontalHeaders(1,"Code Arbre Produit");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(2,"Code Requirement");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(3,"Document Associé");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(4,"Chapitre");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(5,"Titre du Requirement");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(6,"Description");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(7,"Type");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(8,"Statut");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(9,"Source");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(10,"Requirement Parent");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(11,"Niveau de Vérification");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(12,"Méthode de vérification");
    ui->editReqSqlTableWidget->SetHorizontalHeaders(13,"Commentaires");
    ui->editReqSqlTableWidget->showTable("REQUIREMENT", m_SQLManager->currentConnection());
    ui->editReqSqlTableWidget->addRelationalTable(1,QSqlRelation("PT","ID","NAME"));
    ui->editReqSqlTableWidget->addRelationalTable(3,QSqlRelation("DOCUMENT","ID","TITLE"));
    ui->editReqSqlTableWidget->addRelationalTable(4,QSqlRelation("REQ_CHAPTER","ID","CHAPTER"));
    ui->editReqSqlTableWidget->addRelationalTable(7,QSqlRelation("REQ_TYPE","ID","TYPE"));
    ui->editReqSqlTableWidget->addRelationalTable(8,QSqlRelation("REQ_STATUS","ID","STATUS"));
    ui->editReqSqlTableWidget->addRelationalTable(10,QSqlRelation("REQUIREMENT","ID","CODE"));
    ui->editReqSqlTableWidget->addRelationalTable(12,QSqlRelation("REQ_METHOD","ID","METHOD"));
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

    ui->editReqChapterSqlTableWidget->SetHorizontalHeaders(1,"Titre du document associé");
    ui->editReqChapterSqlTableWidget->SetHorizontalHeaders(2,"Nom du chapitre");
    ui->editReqChapterSqlTableWidget->showTable("REQ_CHAPTER", m_SQLManager->currentConnection());
    ui->editReqChapterSqlTableWidget->addRelationalTable(1,QSqlRelation("DOCUMENT","ID","TITLE"));
    ui->editReqChapterSqlTableWidget->setColumnHidden(0,true);

    ui->editConfChangeWidget->SetHorizontalHeaders(1,"Code Requirement");
    ui->editConfChangeWidget->SetHorizontalHeaders(2,"Description");
    ui->editConfChangeWidget->SetHorizontalHeaders(3,"Statut");
    ui->editConfChangeWidget->showTable("CONF_CHANGE",m_SQLManager->currentConnection());
    ui->editConfChangeWidget->addRelationalTable(1,QSqlRelation("REQUIREMENT","ID","CODE"));
    ui->editConfChangeWidget->addRelationalTable(3,QSqlRelation("CC_STATUS","ID","STATUS"));
    ui->editConfChangeWidget->setColumnHidden(0,true);

    ui->editIFWidget->SetHorizontalHeaders(1,"Element n°1");
    ui->editIFWidget->SetHorizontalHeaders(2,"Element n°2");
    ui->editIFWidget->SetHorizontalHeaders(3,"Titre du document associé");
    ui->editIFWidget->SetHorizontalHeaders(4,"Description");
    ui->editIFWidget->SetHorizontalHeaders(5,"Chapitre associé");
    ui->editIFWidget->showTable("INTERFACE",m_SQLManager->currentConnection());
    ui->editIFWidget->addRelationalTable(1,QSqlRelation("PT","ID","NAME"));
    ui->editIFWidget->addRelationalTable(2,QSqlRelation("PT","ID","NAME"));
    ui->editIFWidget->addRelationalTable(3,QSqlRelation("DOCUMENT","ID","TITLE"));
    ui->editIFWidget->addRelationalTable(5,QSqlRelation("IF_CHAPTER","ID","CHAPTER"));
    ui->editIFWidget->setColumnHidden(0,true);

    ui->editIFChaptersWidget->SetHorizontalHeaders(1,"Titre du document");
    ui->editIFChaptersWidget->SetHorizontalHeaders(2,"Nom du chapitre");
    ui->editIFChaptersWidget->showTable("IF_CHAPTER",m_SQLManager->currentConnection());
    ui->editIFChaptersWidget->addRelationalTable(1,QSqlRelation("DOCUMENT","ID","TITLE"));
    ui->editIFChaptersWidget->setColumnHidden(0,true);

    ui->editModelWidget->SetHorizontalHeaders(1,"Modèle instrument");
    ui->editModelWidget->showTable("MODEL",m_SQLManager->currentConnection());
    ui->editModelWidget->setColumnHidden(0,true);

    ui->editPartsWidget->SetHorizontalHeaders(1,"Code Arbre Produit");
    ui->editPartsWidget->SetHorizontalHeaders(2,"Référence du composant");
    ui->editPartsWidget->SetHorizontalHeaders(3,"Numéro de série");
    ui->editPartsWidget->SetHorizontalHeaders(4,"Modèle instrument associé");
    ui->editPartsWidget->SetHorizontalHeaders(5,"Description");
    ui->editPartsWidget->showTable("PART",m_SQLManager->currentConnection());
    ui->editPartsWidget->addRelationalTable(1,QSqlRelation("PT","ID","NAME"));
    ui->editPartsWidget->addRelationalTable(4,QSqlRelation("MODEL","ID","MODEL"));
    ui->editPartsWidget->setColumnHidden(0,true);

}

QVariant MainWindow::getRelatedTablePKvalue(int column,SQLTableForm *tableform, QString searchStr, bool CanBeNull)
{
    //recherche dans la table liée
    QSqlRelation relation = tableform->getRelationAtColumn(column);
    if(!relation.isValid())
        return -1;
    QSqlDatabase db =QSqlDatabase::database(m_SQLManager->currentConnection());
    if(!db.isValid())
        return QVariant();

    if(!db.isOpen())
        return QVariant();

    QSqlQuery query(db);

    query.exec(QString("SELECT * FROM %1 WHERE %2=\"%3\"").arg(relation.tableName(),relation.displayColumn(),searchStr));

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
                QtCSV::StringData strData;
                strData.addRow(_exportCsvDialog->columnNames());
                for (int row=0; row < sqlTable->model()->rowCount();++row)
                {
                    strList.clear();
                    for (int column = 0; column < columnIndexes.count();++column)
                        strList <<  sqlTable->model()->index(row,columnIndexes.at(column)).data().toString();
                    strData.addRow(strList);
                }
                if (QtCSV::Writer::write(filename,strData,";") == false)
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
    refreshEditTables();
    refreshViewTables();
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
    else if(current_tab=="tab_ReqDocChapter_edit")
    {
        importChapter(ui->editReqChapterSqlTableWidget);
    }


}



void MainWindow::importReq()
{
    m_importLog.clear();

    QSqlDatabase db = QSqlDatabase::database(m_SQLManager->currentConnection());

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

    QList<QStringList> data = QtCSV::Reader::readToList(filename, ";", "\"");
    if (data.count()<2)//ne permet pas de changer le séparateur...
        data = QtCSV::Reader::readToList(filename, ",", "\"");
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
}

void MainWindow::importBasic(SQLTableForm *tableform)
{
    m_importLog.clear();

    QSqlDatabase db = QSqlDatabase::database(m_SQLManager->currentConnection());

    if(!db.isValid())
        return;

    if(!db.isOpen())
        return;

    //ouverture du csv
    QString filename = QFileDialog::getOpenFileName(this,"Importation","","*.csv");

    if (filename.isEmpty())
        return;

    QList<QStringList> data = QtCSV::Reader::readToList(filename, ";", "\"");
    if (data.isEmpty())
        data = QtCSV::Reader::readToList(filename, ",", "\"");
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

    QSqlDatabase db = QSqlDatabase::database(m_SQLManager->currentConnection());

    if(!db.isValid())
        return;

    if(!db.isOpen())
        return;

    //ouverture du csv
    QString filename = QFileDialog::getOpenFileName(this,"Importation","","*.csv");

    if (filename.isEmpty())
        return;

    QList<QStringList> data = QtCSV::Reader::readToList(filename, ";", "\"");
    if (data.isEmpty())
        data = QtCSV::Reader::readToList(filename, ",", "\"");
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
    query.exec("SELECT T.DOC_ID,T.CHAPTER FROM "+tableform->tableName());
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
