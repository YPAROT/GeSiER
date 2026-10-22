#include <QComboBox>
#include <QBuffer>
#include <QCheckBox>
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTreeWidget>
#include <QWidget>
#include <QtTest>

#include "producttreeservice.h"
#include "checkablecombobox.h"
#include "req_sqlmanager.h"
#include "requirementwidget.h"
#include "markdowndescriptioneditor.h"
#include "requirementimportdialog.h"
#include "documentcreationdialog.h"
#include "documentwidget.h"
#include "referencedataservice.h"
#include "referencedatawidget.h"

class RequirementWidgetTest : public QObject {
  Q_OBJECT
private slots:
  void editsMarkdownWithPreviewAndToolbar();
  void resolvesMarkdownImagesOnFirstPreview();
  void startsWithoutEditingAndConstrainsPrimaryAllocation();
  void checkableFiltersSupportMultipleValues();
  void relationGraphLayersPlacementAndPreferences();
  void referenceDataWidgetRefreshesCatalogs();
  void parsesTraceRootImportValues();
  void keepsCurrentRequirementWhenDiscardIsRefused();
  void configuresDocumentCreationScope();
  void marksObsoleteRequirementsInDocumentTree();
  void displaysRequirementHistoryAsHumanDiff();
  void refreshesRequirementFilterLookups();
  void displaysLiveRequirementCodeDiagnostics();
};

void RequirementWidgetTest::displaysLiveRequirementCodeDiagnostics() {
  QTemporaryDir directory; REQ_SQLManager manager;
  QVERIFY(manager.newDB(directory.filePath("code-diagnostics.db")).type()==QSqlError::NoError);
  ProductTreeService tree(manager.currentConnection()); QVERIFY(tree.addNode(-1,"SYS").success);
  QSqlQuery query(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'")); QVERIFY(query.next()); const int pt=query.value(0).toInt();
  QVERIFY(query.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'BAD','Test',1,1)"));
  QVERIFY(query.exec(QString("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,%1,1)").arg(pt)));
  RequirementWidget widget; widget.setConnectionName(manager.currentConnection()); widget.openRequirement(1);
  auto *code=widget.findChild<QLineEdit *>("requirementCodeEditor");
  auto *warning=widget.findChild<QLabel *>("requirementCodeWarning"); QVERIFY(code); QVERIFY(warning);
  QCOMPARE(warning->text(),QString("Le code ne correspond pas au format enregistré. Le code est volontairement inchangé."));
  QVERIFY(!warning->toolTip().isEmpty());
  code->setText("SYS-R-0001"); QCoreApplication::processEvents();
  QVERIFY(warning->text().isEmpty()); QVERIFY(warning->toolTip().isEmpty());
  code->setText("SYS-R-0000"); QCoreApplication::processEvents();
  QVERIFY(warning->text().isEmpty()); QVERIFY(warning->toolTip().isEmpty());
  code->setText("SYS-R-01"); QCoreApplication::processEvents();
  QVERIFY(warning->toolTip().contains("exactement 4"));
  widget.setConnectionName({}); manager.close();
}

void RequirementWidgetTest::refreshesRequirementFilterLookups() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  QVERIFY(manager.newDB(directory.filePath("filter-refresh.db")).type()==QSqlError::NoError);
  ProductTreeService tree(manager.currentConnection()); QVERIFY(tree.addNode(-1,"SYS").success);
  QSqlQuery query(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'")); QVERIFY(query.next()); const int pt=query.value(0).toInt();
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(1,%1,1,'Initial')").arg(pt)));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SPEC-1',1)"));
  RequirementWidget widget; widget.setConnectionName(manager.currentConnection());
  auto *documents=widget.findChild<CheckableComboBox *>("documentFilter"); QVERIFY(documents);
  QCOMPARE(documents->toolTip(),QString("Documents : indifférent"));
  documents->setCheckedIds({1});
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) VALUES(2,%1,1,'Importée')").arg(pt)));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(2,'SPEC-2',1)"));
  widget.refreshFilterLookups();
  QCOMPARE(documents->checkedIds(),QList<int>({1}));
  QVERIFY(documents->text().contains("1/2"));
  widget.setConnectionName({}); manager.close();
}

void RequirementWidgetTest::displaysRequirementHistoryAsHumanDiff() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  QVERIFY(manager.newDB(directory.filePath("history-widget.db")).type()==QSqlError::NoError);
  ProductTreeService tree(manager.currentConnection());QVERIFY(tree.addNode(-1,"SYS").success);
  QSqlQuery q(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(q.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));QVERIFY(q.next());const int pt=q.value(0).toInt();
  QVERIFY(q.exec("INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES(1,'REQ-1','Nouveau titre',1,1)"));
  QVERIFY(q.exec(QString("INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,%1,1)").arg(pt)));
  QVERIFY(q.exec("INSERT INTO EVENT_LOG(EVENT_TYPE,OBJECT_TYPE,OBJECT_ID,BEFORE_JSON,AFTER_JSON,PAYLOAD_VERSION) VALUES('UPDATE','REQUIREMENT',1,'{\"fields\":{\"title\":\"Ancien titre\"}}','{\"fields\":{\"title\":\"Nouveau titre\"}}',2)"));
  RequirementWidget widget;widget.setConnectionName(manager.currentConnection());widget.openRequirement(1);
  auto *history=widget.findChild<QTableWidget *>("requirementHistoryTable");QVERIFY(history);QCOMPARE(history->columnCount(),5);QCOMPARE(history->horizontalHeaderItem(3)->text(),QString("Modifications"));QCOMPARE(history->rowCount(),1);
  auto *diff=qobject_cast<QLabel *>(history->cellWidget(0,3));QVERIFY(diff);QVERIFY(diff->text().contains("color:#b42318"));QVERIFY(diff->text().contains("color:#067647"));QVERIFY(diff->text().contains("Nouveau titre"));
}

void RequirementWidgetTest::marksObsoleteRequirementsInDocumentTree() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("document-tree.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  ProductTreeService productTree(manager.currentConnection());
  QVERIFY(productTree.addNode(-1, "SYS").success);
  QSqlQuery query(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));
  QVERIFY(query.next());
  const int ptId = query.value(0).toInt();
  QVERIFY(query.exec(QString(
      "INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
      "VALUES(1,%1,1,'Spécification')")
                         .arg(ptId)));
  QVERIFY(query.exec("INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) VALUES(1,'SPEC-1',1)"));
  QVERIFY(query.exec(QString(
      "INSERT INTO REQUIREMENT(ID,CODE,TITLE,DESCRIPTION,TYPE,STATUS) VALUES"
      "(1,'REQ-ACT','Active','Description active',1,1),"
      "(2,'REQ-OBS','Ancienne','Description obsolète',1,"
      "(SELECT ID FROM REQ_STATUS WHERE UPPER(SHORTCUT)='O' LIMIT 1))")));
  QVERIFY(query.exec(QString(
      "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
      "(1,%1,1),(2,%1,1)")
                         .arg(ptId)));
  DocumentService service(manager.currentConnection());
  QVERIFY(service.placeRequirement(1, -1, 1).success);
  QVERIFY(service.placeRequirement(1, -1, 2).success);

  DocumentWidget widget;
  widget.setConnectionName(manager.currentConnection());
  widget.openDocument(1);
  auto *tree = widget.findChild<QTreeWidget *>("documentStructureTree");
  auto *documents = widget.findChild<QListWidget *>("documentList");
  auto *listPanel = widget.findChild<QWidget *>("documentListPanel");
  auto *splitter = widget.findChild<QSplitter *>("documentSplitter");
  auto *tabs = widget.findChild<QTabWidget *>("documentActionTabs");
  auto *editPage = widget.findChild<QWidget *>("documentEditActions");
  auto *exportPage = widget.findChild<QWidget *>("documentExportActions");
  auto *templatePath = widget.findChild<QLineEdit *>("documentTemplatePath");
  auto *templateGroup =
      widget.findChild<QWidget *>("documentTemplateGroup");
  auto *save = widget.findChild<QPushButton *>("documentSaveButton");
  auto *addRequirement =
      widget.findChild<QPushButton *>("addRequirementToDocumentButton");
  auto *addMetadata =
      widget.findChild<QPushButton *>("addDocumentMetadataButton");
  auto *removeMetadata =
      widget.findChild<QPushButton *>("removeDocumentMetadataButton");
  QVERIFY(tree);
  QVERIFY(documents);
  QVERIFY(listPanel);
  QVERIFY(splitter);
  QVERIFY(tabs);
  QVERIFY(editPage);
  QVERIFY(exportPage);
  QVERIFY(templatePath);
  QVERIFY(templateGroup);
  QVERIFY(save);
  QVERIFY(addRequirement);
  QVERIFY(addMetadata);
  QVERIFY(removeMetadata);
  QCOMPARE(addMetadata->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
  QCOMPARE(removeMetadata->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
  QCOMPARE(addMetadata->text(), QString("+ Ajouter"));
  QCOMPARE(removeMetadata->text(), QString("− Supprimer"));
  QCOMPARE(listPanel->maximumWidth(), 300);
  QCOMPARE(documents->maximumWidth(), 300);
  QVERIFY(documents->wordWrap());
  QCOMPARE(documents->textElideMode(), Qt::ElideNone);
  QCOMPARE(documents->count(), 1);
  QCOMPARE(documents->item(0)->text(), QString("SPEC-1\nSpécification"));
  QVERIFY(documents->item(0)->toolTip().isEmpty());
  QCOMPARE(tabs->count(), 2);
  QCOMPARE(tabs->tabText(0), QString("Édition"));
  QCOMPARE(tabs->tabText(1), QString("Export"));
  QCOMPARE(tabs->widget(0), editPage);
  QCOMPARE(tabs->widget(1), exportPage);
  QVERIFY(templateGroup->isAncestorOf(templatePath));
  QVERIFY(exportPage->isAncestorOf(templateGroup));
  QVERIFY(tabs->isAncestorOf(save));
  QVERIFY(editPage->isAncestorOf(save));
  QVERIFY(!exportPage->isAncestorOf(save));
  QVERIFY(!widget.findChild<QWidget *>("documentContentActions")
               ->isAncestorOf(save));
  QVERIFY(!widget.findChild<QWidget *>("documentStructureActions")
               ->isAncestorOf(save));
  QCOMPARE(widget.findChild<QWidget *>("documentStructureActions")
               ->sizePolicy()
               .verticalPolicy(),
           QSizePolicy::Fixed);
  QCOMPARE(widget.findChild<QWidget *>("documentContentActions")
               ->sizePolicy()
               .verticalPolicy(),
           QSizePolicy::Fixed);
  widget.resize(1200, 800);
  widget.show();
  QCoreApplication::processEvents();
  splitter->setSizes({220, 980});
  QCoreApplication::processEvents();
  QVERIFY(splitter->sizes().first() < 300);
  bool pickerInspected = false;
  QTimer::singleShot(0, [&] {
    auto *picker = qobject_cast<QDialog *>(QApplication::activeModalWidget());
    QVERIFY(picker);
    QCOMPARE(picker->objectName(), QString("documentRequirementPicker"));
    auto *codeFilter =
        picker->findChild<QLineEdit *>("documentRequirementCodeFilter");
    auto *titleFilter =
        picker->findChild<QLineEdit *>("documentRequirementTitleFilter");
    auto *ptFilter =
        picker->findChild<QComboBox *>("documentRequirementPtFilter");
    auto *descriptionFilter = picker->findChild<QLineEdit *>(
        "documentRequirementDescriptionFilter");
    auto *results =
        picker->findChild<QTableWidget *>("documentRequirementResults");
    QVERIFY(codeFilter);
    QVERIFY(titleFilter);
    QVERIFY(ptFilter);
    QVERIFY(descriptionFilter);
    QVERIFY(results);
    QCOMPARE(results->rowCount(), 2);
    codeFilter->setText("OBS");
    QCOMPARE(results->rowCount(), 1);
    codeFilter->clear();
    titleFilter->setText("Active");
    QCOMPARE(results->rowCount(), 1);
    titleFilter->clear();
    descriptionFilter->setText("obsolète");
    QCOMPARE(results->rowCount(), 1);
    descriptionFilter->clear();
    ptFilter->setCurrentIndex(ptFilter->findData(ptId));
    QCOMPARE(results->rowCount(), 2);
    pickerInspected = true;
    picker->reject();
  });
  QTest::mouseClick(addRequirement, Qt::LeftButton);
  QVERIFY(pickerInspected);
  QCOMPARE(tree->topLevelItemCount(), 2);
  QCOMPARE(tree->topLevelItem(0)->text(0), QString("REQ-ACT — Active"));
  QCOMPARE(tree->topLevelItem(0)->foreground(0).color(),
           widget.palette().brush(QPalette::Link).color());
  QCOMPARE(tree->topLevelItem(1)->text(0),
           QString("REQ-OBS — Ancienne [Obsolete]"));
  QCOMPARE(tree->topLevelItem(1)->foreground(0).color(), QColor("#808080"));
  int obsoleteNodeId = -1;
  for (const DocumentNodeRecord &node : service.nodes(1))
    if (node.requirementId == 2)
      obsoleteNodeId = node.id;
  QVERIFY(obsoleteNodeId >= 0);
  QVERIFY(service.moveNode(obsoleteNodeId, -1, 0).success);
  widget.setConnectionName(manager.currentConnection());
  widget.openDocument(1);
  QCOMPARE(tree->topLevelItem(0)->text(0),
           QString("REQ-OBS — Ancienne [Obsolete]"));
  QCOMPARE(tree->topLevelItem(1)->text(0), QString("REQ-ACT — Active"));
  widget.releaseDatabase();
  manager.close();
}

void RequirementWidgetTest::configuresDocumentCreationScope() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const auto creation = manager.newDB(directory.filePath("document-dialog.db"));
  QVERIFY2(creation.type() == QSqlError::NoError, qPrintable(creation.text()));
  DocumentCreationDialog dialog(manager.currentConnection());
  auto *attach = dialog.findChild<QCheckBox *>("attachPtRequirements");
  auto *children =
      dialog.findChild<QCheckBox *>("includeChildPtRequirements");
  QVERIFY(attach);
  QVERIFY(children);
  QVERIFY(attach->isChecked());
  QVERIFY(attach->isEnabled());
  QVERIFY(!children->isChecked());
  children->setChecked(true);
  QVERIFY(attach->isChecked());
  QVERIFY(!attach->isEnabled());
  children->setChecked(false);
  QVERIFY(attach->isEnabled());
  manager.close();
}

void RequirementWidgetTest::resolvesMarkdownImagesOnFirstPreview() {
  const QByteArray png = QByteArray::fromBase64(
      "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=");
  RequirementImage first{"first", "image/png", {}, png};
  RequirementImage second{"second", "image/png", {}, png};
  QImage largeImage(1458, 1128, QImage::Format_ARGB32);
  largeImage.fill(Qt::white);
  QByteArray largePng;
  QBuffer largeBuffer(&largePng);
  QVERIFY(largeBuffer.open(QIODevice::WriteOnly));
  QVERIFY(largeImage.save(&largeBuffer, "PNG"));
  RequirementImage large{"large", "image/png", {}, largePng};
  MarkdownDescriptionEditor editor;
  editor.resize(700, 500);
  editor.setContent(
      "![](gesier-image://first)\n\n![](gesier-image://second)\n\n"
      "![](gesier-image://large)\n\n![](gesier-image://missing)\n\n"
      "![](https://example.invalid/image.png)",
      {first, second, large});
  auto *preview = editor.findChild<QTextBrowser *>("markdownPreview");
  QVERIFY(preview);
  bool foundFirstImage = false;
  for (QTextBlock block = preview->document()->begin(); block.isValid();
       block = block.next()) {
    for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
      if (fragment.fragment().charFormat().objectType() ==
              QTextFormat::ImageObject &&
          fragment.fragment().charFormat().toImageFormat().name() ==
              "gesier-image://first")
        foundFirstImage = true;
    }
  }
  QVERIFY(foundFirstImage);
  const auto resource = [&](const QString &url) {
    return preview->document()
        ->resource(QTextDocument::ImageResource, QUrl(url))
        .value<QImage>();
  };
  QCOMPARE(resource("gesier-image://first").size(), QSize(1, 1));
  QCOMPARE(resource("gesier-image://second").size(), QSize(1, 1));
  QVERIFY(resource("gesier-image://large").width() <=
          preview->viewport()->width());
  QVERIFY(resource("gesier-image://large").height() > 0);
  QCOMPARE(resource("gesier-image://missing").size(), QSize(240, 48));
  QVERIFY(resource("https://example.invalid/image.png").isNull());

  editor.setContent("![](gesier-image://late)");
  QCOMPARE(resource("gesier-image://late").size(), QSize(240, 48));
  RequirementImage late{"late", "image/png", {}, png};
  editor.setContent("![](gesier-image://late)", {late});
  QCOMPARE(resource("gesier-image://late").size(), QSize(1, 1));
}

void RequirementWidgetTest::editsMarkdownWithPreviewAndToolbar() {
  MarkdownDescriptionEditor editor;
  editor.setContent("Texte");
  QVERIFY(!editor.isEditing());
  auto *toggle = editor.findChild<QPushButton *>("markdownModeToggle");
  auto *source = editor.findChild<QPlainTextEdit *>("markdownSource");
  auto *bold = editor.findChild<QPushButton *>("markdownBold");
  QVERIFY(toggle); QVERIFY(source); QVERIFY(bold);
  toggle->click();
  QVERIFY(editor.isEditing());
  source->selectAll();
  bold->click();
  QCOMPARE(editor.markdown(), QString("**Texte**"));
  toggle->click();
  QVERIFY(!editor.isEditing());
  QCOMPARE(MarkdownDescriptionEditor::plainText(editor.markdown()),
           QString("Texte"));
}

void RequirementWidgetTest::parsesTraceRootImportValues() {
  bool value = false;
  for (const QString &text : {"oui", "TRUE", "1", "Racine", "root"}) {
    QVERIFY(RequirementImportDialog::parseTraceRootValue(text, &value));
    QVERIFY(value);
  }
  for (const QString &text : {"", "non", "FALSE", "0"}) {
    QVERIFY(RequirementImportDialog::parseTraceRootValue(text, &value));
    QVERIFY(!value);
  }
  QVERIFY(!RequirementImportDialog::parseTraceRootValue("peut-être", &value));
  QVERIFY(!RequirementImportDialog::parseTraceRootValue("oui", nullptr));
}

void RequirementWidgetTest::keepsCurrentRequirementWhenDiscardIsRefused() {
  QTemporaryDir directory;
  REQ_SQLManager manager;
  const QSqlError error = manager.newDB(directory.filePath("keyboard.db"));
  QVERIFY2(error.type() == QSqlError::NoError, qPrintable(error.text()));
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "SYS").success);
  QSqlQuery query(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));
  QVERIFY(query.next());
  const int ptId = query.value(0).toInt();
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
                             "VALUES(1,%1,1,'Keyboard')")
                         .arg(ptId)));
  QVERIFY(query.exec(QString(
      "INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS,COMMENTS) "
      "VALUES(1,'REQ-1','One',1,1,'Avis UI'),(2,'REQ-2','Two',1,1,'')")));
  QVERIFY(query.exec(QString(
      "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
      "(1,%1,1),(2,%1,1)")
                         .arg(ptId)));

  RequirementWidget widget;
  widget.setConnectionName(manager.currentConnection());
  auto *list = widget.findChild<QTableWidget *>("requirementList");
  auto *title = widget.findChild<QLineEdit *>("requirementTitleEditor");
  auto *comments = widget.findChild<QTextEdit *>("requirementCommentsEditor");
  QVERIFY(list); QVERIFY(title); QVERIFY(comments);
  widget.openRequirement(1);
  QCOMPARE(comments->toPlainText(), QString("Avis UI"));
  const int initialRow = list->currentRow();
  QVERIFY(initialRow >= 0);
  title->setFocus();
  QTest::keyClick(title, Qt::Key_A, Qt::ControlModifier);
  QTest::keyClicks(title, "Modification locale");
  list->setFocus();
  QTimer::singleShot(0, [] {
    if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
      box->done(QMessageBox::No);
  });
  QTest::keyClick(list, initialRow == 0 ? Qt::Key_Down : Qt::Key_Up);
  QTRY_COMPARE(list->currentRow(), initialRow);
  QCOMPARE(widget.findChild<QLineEdit *>("requirementCodeEditor")->text(),
           QString("REQ-1"));
  QCOMPARE(title->text(), QString("Modification locale"));
  manager.close();
}

void RequirementWidgetTest::referenceDataWidgetRefreshesCatalogs() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  REQ_SQLManager manager;
  const QSqlError error = manager.newDB(directory.filePath("settings.db"));
  QVERIFY2(error.type() == QSqlError::NoError, qPrintable(error.text()));
  ReferenceDataService service(manager.currentConnection());
  QVERIFY(service.addVerificationMethod("Inspection UI").success);
  QVERIFY(service.addRequirementType("UI-CUSTOM", "Type UI personnalisé").success);

  ReferenceDataWidget widget;
  widget.setConnectionName(manager.currentConnection());
  auto *methods = widget.findChild<QTableWidget *>("verificationMethodsTable");
  auto *types = widget.findChild<QTableWidget *>("requirementTypesTable");
  QVERIFY(methods);
  QVERIFY(types);
  auto *preview = widget.findChild<QLabel *>("requirementCodePreview");
  auto *includeRoot = widget.findChild<QCheckBox *>("requirementCodeIncludeRoot");
  QVERIFY(preview); QVERIFY(includeRoot);
  QCOMPARE(preview->text(), QString("RACINE-PTLVL1-R-0000"));
  includeRoot->setChecked(false);
  QCOMPARE(preview->text(), QString("PTLVL1-R-0000"));
  bool foundMethod = false, foundType = false;
  for (int row = 0; row < methods->rowCount(); ++row)
    foundMethod |= methods->item(row, 0)->text() == "Inspection UI";
  for (int row = 0; row < types->rowCount(); ++row)
    foundType |= types->item(row, 0)->text() == "UI-CUSTOM" &&
                 types->item(row, 1)->text() == "Type UI personnalisé";
  QVERIFY(foundMethod);
  QVERIFY(foundType);
  widget.releaseDatabase();
  QCOMPARE(methods->rowCount(), 0);
  QCOMPARE(types->rowCount(), 0);
  manager.close();
}

void RequirementWidgetTest::relationGraphLayersPlacementAndPreferences() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                     directory.path());
  QSettings settings;
  settings.remove("Requirements/graph/showDerivations");
  settings.remove("Requirements/graph/showDependencies");
  settings.sync();
  REQ_SQLManager manager;
  const QSqlError error = manager.newDB(directory.filePath("graph.db"));
  QVERIFY2(error.type() == QSqlError::NoError, qPrintable(error.text()));
  ProductTreeService tree(manager.currentConnection());
  QVERIFY(tree.addNode(-1, "SYS").success);
  QSqlQuery query(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='SYS'"));
  QVERIFY(query.next());
  const int ptId = query.value(0).toInt();
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
                             "VALUES(1,%1,1,'Graph')")
                         .arg(ptId)));
  QStringList values;
  for (int requirementId = 1; requirementId <= 8; ++requirementId)
    values << QString("(%1,'REQ-%1','Requirement %1',1,1)")
                  .arg(requirementId);
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) VALUES" + values.join(',')));
  QVERIFY(query.exec("UPDATE REQUIREMENT SET IS_TRACE_ROOT=1 WHERE ID=2"));
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID) "
      "VALUES(2,1,1),(2,3,1),(3,4,1),(1,5,2),(5,6,2),(3,7,3),"
      "(3,8,1),(6,8,3)"));

  RequirementWidget widget;
  widget.setConnectionName(manager.currentConnection());
  auto *view = widget.findChild<QGraphicsView *>();
  auto *depth = widget.findChild<QSpinBox *>("relationGraphDepth");
  auto *derivations =
      widget.findChild<QCheckBox *>("showRelationDerivations");
  auto *dependencies =
      widget.findChild<QCheckBox *>("showRelationDependencies");
  QVERIFY(view); QVERIFY(depth); QVERIFY(derivations); QVERIFY(dependencies);
  QVERIFY(derivations->isChecked());
  QVERIFY(dependencies->isChecked());
  depth->setValue(3);
  widget.openRequirement(1);

  auto node = [view](int requirementId) -> QGraphicsRectItem * {
    for (QGraphicsItem *item : view->scene()->items()) {
      if (item->data(1).toString() == "node" &&
          item->data(0).toInt() == requirementId)
        return qgraphicsitem_cast<QGraphicsRectItem *>(item);
    }
    return nullptr;
  };
  QVERIFY(node(1)); QVERIFY(node(2)); QVERIFY(node(3)); QVERIFY(node(4));
  QVERIFY(node(5)); QVERIFY(node(6)); QVERIFY(node(7)); QVERIFY(node(8));
  QVERIFY(node(1)->toolTip().contains("REQ-1 — Requirement 1"));
  QVERIFY(node(1)->toolTip().contains("Cliquer pour ouvrir"));
  QVERIFY(!node(1)->toolTip().contains("Exigence root"));
  QCOMPARE(node(1)->pen().widthF(), 1.0);
  QVERIFY(node(2)->toolTip().contains("REQ-2 — Requirement 2"));
  QVERIFY(node(2)->toolTip().contains("Exigence root"));
  QCOMPARE(node(2)->pen().color(), QColor("#6f42c1"));
  QCOMPARE(node(2)->pen().widthF(), 3.0);
  int rootBadgeCount = 0;
  for (QGraphicsItem *item : view->scene()->items())
    if (item->data(1).toString() == "rootBadge" && item->data(0).toInt() == 2) {
      QVERIFY(qgraphicsitem_cast<QGraphicsEllipseItem *>(item));
      QVERIFY(item->acceptedMouseButtons() == Qt::NoButton);
      ++rootBadgeCount;
    }
  QCOMPARE(rootBadgeCount, 1);
  const QPointF focus = node(1)->sceneBoundingRect().center();
  QVERIFY(node(2)->sceneBoundingRect().center().y() < focus.y());
  QCOMPARE(node(3)->sceneBoundingRect().center().y(), focus.y());
  QVERIFY(node(3)->sceneBoundingRect().center().x() != focus.x());
  QVERIFY(node(4)->sceneBoundingRect().center().y() >
          node(3)->sceneBoundingRect().center().y());
  QVERIFY(node(5)->sceneBoundingRect().center().x() < focus.x());
  QVERIFY(node(6)->sceneBoundingRect().center().x() <
          node(5)->sceneBoundingRect().center().x());
  QVERIFY(node(7)->sceneBoundingRect().center().x() >
          node(3)->sceneBoundingRect().center().x());
  QCOMPARE(node(7)->sceneBoundingRect().center().y(),
           node(3)->sceneBoundingRect().center().y());

  int edgesToSharedNode = 0;
  int sharedNodeCount = 0;
  for (QGraphicsItem *item : view->scene()->items())
    if (item->data(1).toString() == "node" && item->data(0).toInt() == 8)
      ++sharedNodeCount;
  QCOMPARE(sharedNodeCount, 1);
  for (QGraphicsItem *item : view->scene()->items()) {
    if (item->data(1).toString() != "edge")
      continue;
    auto *line = qgraphicsitem_cast<QGraphicsLineItem *>(item);
    QVERIFY(line);
    QGraphicsRectItem *source = node(item->data(3).toInt());
    QGraphicsRectItem *target = node(item->data(4).toInt());
    QVERIFY(source); QVERIFY(target);
    const QLineF edge = line->line();
    auto onBoundary = [](const QPointF &point, const QRectF &box) {
      const qreal epsilon = 0.01;
      return (qAbs(point.x() - box.left()) < epsilon ||
              qAbs(point.x() - box.right()) < epsilon ||
              qAbs(point.y() - box.top()) < epsilon ||
              qAbs(point.y() - box.bottom()) < epsilon) &&
             box.adjusted(-epsilon, -epsilon, epsilon, epsilon).contains(point);
    };
    QVERIFY(onBoundary(edge.p1(), source->rect()));
    QVERIFY(onBoundary(edge.p2(), target->rect()));
    if (item->data(4).toInt() == 8)
      ++edgesToSharedNode;
    if (item->data(2).toString() == "DERIVES_FROM" &&
        item->data(3).toInt() == 1)
      QCOMPARE(item->data(4).toInt(), 5);
    if (item->data(2).toString() == "DEPENDS_ON" &&
        item->data(3).toInt() == 3)
      QCOMPARE(item->data(4).toInt(), 7);
  }
  QCOMPARE(edgesToSharedNode, 2);

  derivations->setChecked(false);
  QVERIFY(!node(5)); QVERIFY(!node(6));
  QVERIFY(node(2)); QVERIFY(node(3)); QVERIFY(node(4)); QVERIFY(node(8));
  dependencies->setChecked(false);
  QVERIFY(!node(7));
  QVERIFY(node(2)); QVERIFY(node(3)); QVERIFY(node(4)); QVERIFY(node(8));
  QCOMPARE(settings.value("Requirements/graph/showDerivations").toBool(),
           false);
  QCOMPARE(settings.value("Requirements/graph/showDependencies").toBool(),
           false);

  RequirementWidget restored;
  QVERIFY(!restored.findChild<QCheckBox *>("showRelationDerivations")
               ->isChecked());
  QVERIFY(!restored.findChild<QCheckBox *>("showRelationDependencies")
               ->isChecked());
  manager.close();
}

void RequirementWidgetTest::checkableFiltersSupportMultipleValues() {
  CheckableComboBox combo("Types");
  combo.addItem("PERF", 1);
  combo.addItem("FUNC", 2);
  QCOMPARE(combo.text(), QString("Types (0/2)"));
  combo.setCheckedIds({1, 2});
  QCOMPARE(combo.checkedIds(), QList<int>({1, 2}));
  QCOMPARE(combo.text(), QString("Types (2/2)"));
  combo.setCheckedIds({2});
  QCOMPARE(combo.checkedIds(), QList<int>({2}));
  QVERIFY(combo.toolTip().contains("FUNC"));
}

void RequirementWidgetTest::
    startsWithoutEditingAndConstrainsPrimaryAllocation() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  REQ_SQLManager manager;
  QSqlError error = manager.newDB(directory.filePath("widget.db"));
  QVERIFY2(error.type() == QSqlError::NoError, qPrintable(error.text()));
  ProductTreeService pt(manager.currentConnection());
  QVERIFY(pt.addNode(-1, "A").success);
  QSqlQuery query(QSqlDatabase::database(manager.currentConnection()));
  QVERIFY(query.exec("SELECT ID FROM PT WHERE SEGMENT='A'"));
  QVERIFY(query.next());
  const int rootId = query.value(0).toInt();
  QVERIFY(pt.addNode(rootId, "B").success);
  QVERIFY(pt.addNode(rootId, "C").success);
  QVERIFY(query.exec(QString("INSERT INTO DOCUMENT(ID,PT_ID,TYPE,TITLE) "
                             "VALUES(1,%1,1,'Test document')")
                         .arg(rootId)));
  QVERIFY(query.exec(
      "INSERT INTO DOCUMENT_REFERENCE(DOC_ID,REFERENCE,IS_PRIMARY) "
      "VALUES(1,'SPEC-001',1)"));
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT(ID,CODE,TITLE,TYPE,STATUS) "
      "VALUES(1,'A-R-0001','Existing requirement',1,1),"
      "(2,'A-R-0002','Keyboard requirement',1,1)"));
  QVERIFY(query.exec(
      QString(
          "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
          "(1,%1,1),(2,%1,1)")
          .arg(rootId)));
  QVERIFY(query.exec("UPDATE REQUIREMENT SET IS_TRACE_ROOT=1 WHERE ID=2"));
  QVERIFY(query.exec(
      "INSERT INTO DOCUMENT_NODE(DOC_ID,NODE_TYPE,POSITION,REQ_ID) "
      "VALUES(1,'REQUIREMENT',0,1)"));

  RequirementWidget widget;
  widget.setConnectionName(manager.currentConnection());
  QWidget *editor = widget.findChild<QWidget *>("requirementEditor");
  QTableWidget *list = widget.findChild<QTableWidget *>("requirementList");
  QTableWidget *filters =
      widget.findChild<QTableWidget *>("requirementFilters");
  QPushButton *create = widget.findChild<QPushButton *>("newRequirementButton");
  QTreeWidget *tree = widget.findChild<QTreeWidget *>("allocationProductTree");
  QComboBox *primary = widget.findChild<QComboBox *>("primaryProductTree");
  auto *documentFilter =
      widget.findChild<CheckableComboBox *>("documentFilter");
  QVERIFY(editor);
  QVERIFY(list);
  QVERIFY(filters);
  QVERIFY(create);
  QVERIFY(tree);
  QVERIFY(primary);
  QVERIFY(documentFilter);
  QVERIFY(!editor->isEnabled());
  QCOMPARE(list->rowCount(), 2);
  QCOMPARE(filters->rowCount(), 1);
  QVERIFY(filters->height() < list->height());
  for (int column = 0; column < list->columnCount(); ++column)
    QCOMPARE(list->horizontalHeader()->sectionResizeMode(column),
             QHeaderView::Interactive);
  constexpr int documentColumn = 6;
  QCOMPARE(list->columnCount(), 13);
  QCOMPARE(filters->columnCount(), 13);
  QCOMPARE(list->item(0, documentColumn)->text(), QString("SPEC-001"));
  QCOMPARE(list->item(0, documentColumn)->toolTip(),
           QString("SPEC-001 — Test document"));
  bool documentTitleFound = false;
  QVERIFY(documentFilter->menu()->toolTipsVisible());
  for (QAction *action : documentFilter->menu()->actions()) {
    if (action->isCheckable() && action->text() == "SPEC-001") {
      QCOMPARE(action->toolTip(), QString("Test document"));
      documentTitleFound = true;
    }
  }
  QVERIFY(documentTitleFound);
  RequirementFilter byDocument;
  byDocument.documentIds = {1};
  widget.applyFilter(byDocument);
  QCOMPARE(list->rowCount(), 1);
  QCOMPARE(list->item(0, 0)->text(), QString("A-R-0001"));
  widget.applyFilter({});
  QCOMPARE(list->rowCount(), 2);
  QVERIFY(list->selectedItems().isEmpty());
  QCOMPARE(tree->topLevelItemCount(), 1);
  QCOMPARE(tree->topLevelItem(0)->text(0).section(" — ", 0, 0), QString("A"));
  QCOMPARE(tree->topLevelItem(0)->childCount(), 2);
  QCOMPARE(tree->topLevelItem(0)->child(0)->text(0).section(" — ", 0, 0),
           QString("A-B"));
  QCOMPARE(tree->topLevelItem(0)->child(1)->text(0).section(" — ", 0, 0),
           QString("A-C"));

  widget.openRequirement(1);
  QVERIFY(editor->isEnabled());
  QCOMPARE(list->currentRow(), 0);
  list->setFocus();
  QTest::keyClick(list, Qt::Key_Down);
  QCoreApplication::processEvents();
  QCOMPARE(list->currentRow(), 1);
  QCOMPARE(widget.findChild<QLineEdit *>("requirementCodeEditor")->text(),
           QString("A-R-0002"));
  QVERIFY(widget.findChild<QCheckBox *>("traceRootCheckBox")->isChecked());

  QTest::mouseClick(create, Qt::LeftButton);
  QVERIFY(editor->isEnabled());
  tree->topLevelItem(0)->setCheckState(0, Qt::Checked);
  QCOMPARE(primary->count(), 1);
  QVERIFY(!primary->isEnabled());
  tree->topLevelItem(0)->child(0)->setCheckState(0, Qt::Checked);
  QCOMPARE(primary->count(), 2);
  QVERIFY(primary->isEnabled());
  tree->topLevelItem(0)->setCheckState(0, Qt::Unchecked);
  QCOMPARE(primary->count(), 1);
  QVERIFY(!primary->isEnabled());
  manager.close();
}

QTEST_MAIN(RequirementWidgetTest)
#include "requirement_widget_test.moc"
