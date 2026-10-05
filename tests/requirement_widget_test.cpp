#include <QComboBox>
#include <QCheckBox>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QWidget>
#include <QtTest>

#include "producttreeservice.h"
#include "checkablecombobox.h"
#include "req_sqlmanager.h"
#include "requirementwidget.h"
#include "requirementimportdialog.h"
#include "referencedataservice.h"
#include "referencedatawidget.h"

class RequirementWidgetTest : public QObject {
  Q_OBJECT
private slots:
  void startsWithoutEditingAndConstrainsPrimaryAllocation();
  void checkableFiltersSupportMultipleValues();
  void relationGraphLayersPlacementAndPreferences();
  void referenceDataWidgetRefreshesCatalogs();
  void parsesTraceRootImportValues();
  void keepsCurrentRequirementWhenDiscardIsRefused();
};

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
      "INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,"
      "VERIF_METHOD) VALUES(1,%1,'REQ-1',1,'One',1,1,1),"
      "(2,%1,'REQ-2',1,'Two',1,1,1)")
                         .arg(ptId)));
  QVERIFY(query.exec(QString(
      "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
      "(1,%1,1),(2,%1,1)")
                         .arg(ptId)));

  RequirementWidget widget;
  widget.setConnectionName(manager.currentConnection());
  auto *list = widget.findChild<QTableWidget *>("requirementList");
  auto *title = widget.findChild<QLineEdit *>("requirementTitleEditor");
  QVERIFY(list); QVERIFY(title);
  widget.openRequirement(1);
  title->setText("Modification locale");
  list->setFocus();
  QTimer::singleShot(0, [] {
    if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
      box->done(QMessageBox::No);
  });
  QTest::keyClick(list, Qt::Key_Down);
  QCOMPARE(list->currentRow(), 1);
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
    values << QString("(%1,%2,'REQ-%1',1,'Requirement %1',1,1,1)")
                  .arg(requirementId)
                  .arg(ptId);
  QVERIFY(query.exec(
      "INSERT INTO REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,"
      "VERIF_METHOD) VALUES" + values.join(',')));
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
      QString("INSERT INTO "
              "REQUIREMENT(ID,PT_ID,CODE,DOC_ID,TITLE,TYPE,STATUS,VERIF_METHOD)"
              " VALUES(1,%1,'A-R-0001',1,'Existing requirement',1,1,1),"
              "(2,%1,'A-R-0002',1,'Keyboard requirement',1,1,1)")
          .arg(rootId)));
  QVERIFY(query.exec(
      QString(
          "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES"
          "(1,%1,1),(2,%1,1)")
          .arg(rootId)));
  QVERIFY(query.exec("UPDATE REQUIREMENT SET IS_TRACE_ROOT=1 WHERE ID=2"));

  RequirementWidget widget;
  widget.setConnectionName(manager.currentConnection());
  QWidget *editor = widget.findChild<QWidget *>("requirementEditor");
  QTableWidget *list = widget.findChild<QTableWidget *>("requirementList");
  QPushButton *create = widget.findChild<QPushButton *>("newRequirementButton");
  QTreeWidget *tree = widget.findChild<QTreeWidget *>("allocationProductTree");
  QComboBox *primary = widget.findChild<QComboBox *>("primaryProductTree");
  QVERIFY(editor);
  QVERIFY(list);
  QVERIFY(create);
  QVERIFY(tree);
  QVERIFY(primary);
  QVERIFY(!editor->isEnabled());
  QCOMPARE(list->rowCount(), 3); // filter row plus the two requirements
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
  QCOMPARE(list->currentRow(), 1);
  list->setFocus();
  QTest::keyClick(list, Qt::Key_Down);
  QCoreApplication::processEvents();
  QCOMPARE(list->currentRow(), 2);
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
