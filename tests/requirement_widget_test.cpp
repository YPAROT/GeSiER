#include <QComboBox>
#include <QPushButton>
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

class RequirementWidgetTest : public QObject {
  Q_OBJECT
private slots:
  void startsWithoutEditingAndConstrainsPrimaryAllocation();
  void checkableFiltersSupportMultipleValues();
};

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
              " VALUES(1,%1,'A-R-0001',1,'Existing requirement',1,1,1)")
          .arg(rootId)));
  QVERIFY(query.exec(
      QString(
          "INSERT INTO REQUIREMENT_PT(REQ_ID,PT_ID,IS_PRIMARY) VALUES(1,%1,1)")
          .arg(rootId)));

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
  QCOMPARE(list->rowCount(), 2); // filter row plus the existing requirement
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
