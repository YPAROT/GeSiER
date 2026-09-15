#include "interfacewidget.h"
#include "n2matrixwidget.h"
#include <QtSql>
#include <QtWidgets>

namespace {
void fillCombo(QComboBox *c, QSqlDatabase db, const QString &sql,
               const QString &all) {
  QSignalBlocker b(c);
  QVariant old = c->currentData();
  c->clear();
  c->addItem(all, -1);
  QSqlQuery q(db);
  if (q.exec(sql))
    while (q.next())
      c->addItem(q.value(1).toString(), q.value(0));
  int i = c->findData(old);
  c->setCurrentIndex(i < 0 ? 0 : i);
}
QListWidget *checkList(QSqlDatabase db, const QString &sql,
                       const QList<int> &selected, QWidget *parent) {
  auto *w = new QListWidget(parent);
  QSqlQuery q(db);
  if (q.exec(sql))
    while (q.next()) {
      auto *i = new QListWidgetItem(q.value(1).toString(), w);
      i->setData(Qt::UserRole, q.value(0));
      i->setFlags(i->flags() | Qt::ItemIsUserCheckable);
      i->setCheckState(selected.contains(q.value(0).toInt()) ? Qt::Checked
                                                             : Qt::Unchecked);
    }
  return w;
}
QList<int> checked(QListWidget *w) {
  QList<int> ids;
  for (int i = 0; i < w->count(); ++i)
    if (w->item(i)->checkState() == Qt::Checked)
      ids << w->item(i)->data(Qt::UserRole).toInt();
  return ids;
}
} // namespace

InterfaceWidget::InterfaceWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  m_tabs = new QTabWidget(this);
  auto *listPage = new QWidget;
  auto *vl = new QVBoxLayout(listPage);
  auto *tools = new QHBoxLayout;
  m_search = new QLineEdit;
  m_search->setPlaceholderText("Code, description ou élément PT…");
  m_search->setClearButtonEnabled(true);
  m_pt = new QComboBox;
  m_type = new QComboBox;
  m_archived = new QCheckBox("Inclure les archivées");
  m_withoutIcd = new QCheckBox("Sans ICD");
  auto *add = new QPushButton("Nouvelle interface");
  auto *types = new QPushButton("Catalogue des types");
  auto *exportButton = new QPushButton("Exporter XLSX…");
  tools->addWidget(m_search, 2);
  tools->addWidget(new QLabel("Élément PT"));
  tools->addWidget(m_pt);
  tools->addWidget(new QLabel("Type"));
  tools->addWidget(m_type);
  tools->addWidget(m_withoutIcd);
  tools->addWidget(m_archived);
  tools->addWidget(types);
  tools->addWidget(exportButton);
  tools->addWidget(add);
  vl->addLayout(tools);
  m_summary = new QLabel;
  vl->addWidget(m_summary);
  m_table = new QTableWidget;
  m_table->setObjectName("interfaceTable");
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setSelectionMode(QAbstractItemView::SingleSelection);
  m_table->setAlternatingRowColors(true);
  m_table->setSortingEnabled(true);
  vl->addWidget(m_table, 1);
  m_n2 = new N2MatrixWidget;
  m_tabs->addTab(listPage, "Liste et fiches");
  m_tabs->addTab(m_n2, "Matrice N²");
  layout->addWidget(m_tabs);
  connect(add, &QPushButton::clicked, this, [this] { edit(); });
  connect(types, &QPushButton::clicked, this, &InterfaceWidget::manageTypes);
  connect(m_search, &QLineEdit::textChanged, this, &InterfaceWidget::refresh);
  for (auto *c : {m_pt, m_type})
    connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &InterfaceWidget::refresh);
  connect(m_archived, &QCheckBox::toggled, this, &InterfaceWidget::refresh);
  connect(m_withoutIcd, &QCheckBox::toggled, this, &InterfaceWidget::refresh);
  connect(m_table, &QTableWidget::cellDoubleClicked, this,
          [this](int row, int) {
            if (auto *i = m_table->item(row, 0))
              edit(i->data(Qt::UserRole).toInt());
          });
  connect(m_n2, &N2MatrixWidget::pairSelected, this, [this](int a, int) {
    applyPtFilter(a);
    m_tabs->setCurrentIndex(0);
  });
  connect(m_n2, &N2MatrixWidget::interfaceRequested, this,
          &InterfaceWidget::openInterface);
  connect(exportButton, &QPushButton::clicked, this, [this] {
    QString path = QFileDialog::getSaveFileName(this, "Exporter les interfaces",
                                                "interfaces-n2.xlsx",
                                                "Classeur Excel (*.xlsx)");
    if (path.isEmpty())
      return;
    if (!path.endsWith(".xlsx", Qt::CaseInsensitive))
      path += ".xlsx";
    auto r = m_service.exportXlsx(path, filter());
    if (r.success)
      QMessageBox::information(this, "Export XLSX", r.message);
    else
      QMessageBox::warning(this, "Export XLSX", r.message);
  });
}
void InterfaceWidget::setConnectionName(const QString &name) {
  m_connection = name;
  m_service.setConnectionName(name);
  m_n2->setConnectionName(name);
  refresh();
}
InterfaceFilter InterfaceWidget::filter() const {
  InterfaceFilter f;
  f.text = m_search->text();
  f.ptId = m_pt->currentData().toInt();
  f.typeId = m_type->currentData().toInt();
  f.includeArchived = m_archived->isChecked();
  f.onlyWithoutIcd = m_withoutIcd->isChecked();
  return f;
}
void InterfaceWidget::refresh() {
  QSqlDatabase db = QSqlDatabase::contains(m_connection)
                        ? QSqlDatabase::database(m_connection, false)
                        : QSqlDatabase();
  if (!db.isValid() || !db.isOpen()) {
    m_table->setRowCount(0);
    m_summary->setText("Aucun projet ouvert.");
    return;
  }
  fillCombo(m_pt, db,
            "SELECT ID,NAME FROM PT WHERE COALESCE(ARCHIVED,0)=0 ORDER BY "
            "POSITION,ID",
            "Tous");
  fillCombo(m_type, db, "SELECT ID,LABEL FROM INTERFACE_TYPE ORDER BY LABEL",
            "Tous");
  QString error;
  m_rows = m_service.find(filter(), &error);
  m_table->setSortingEnabled(false);
  m_table->setColumnCount(9);
  m_table->setHorizontalHeaderLabels({"Code", "Élément 1", "Élément 2",
                                      "Statut", "Types", "Exigences", "ICD",
                                      "Description", "État"});
  m_table->setRowCount(m_rows.size());
  int covered = 0;
  for (int r = 0; r < m_rows.size(); ++r) {
    const auto &x = m_rows[r];
    if (!x.documents.isEmpty())
      covered++;
    QStringList v{
        x.code,      x.element1,    x.element2,
        x.status,    x.types,       x.requirements,
        x.documents, x.description, x.archived ? "Archivée" : "Active"};
    for (int c = 0; c < v.size(); ++c)
      m_table->setItem(r, c, new QTableWidgetItem(v[c]));
    m_table->item(r, 0)->setData(Qt::UserRole, x.id);
    if (x.documents.isEmpty())
      for (int c = 0; c < m_table->columnCount(); ++c)
        m_table->item(r, c)->setBackground(QColor("#fff1cc"));
    if (x.archived)
      for (int c = 0; c < m_table->columnCount(); ++c)
        m_table->item(r, c)->setForeground(QColor("#777777"));
  }
  m_table->setSortingEnabled(true);
  m_table->horizontalHeader()->setStretchLastSection(true);
  m_summary->setText(
      error.isEmpty()
          ? QString("%1 interface(s) — couverture ICD de la sélection : %2 %")
                .arg(m_rows.size())
                .arg(m_rows.isEmpty() ? 0. : 100. * covered / m_rows.size(), 0,
                     'f', 2)
          : error);
  m_n2->refresh();
}
void InterfaceWidget::applyPtFilter(int id) {
  int i = m_pt->findData(id);
  if (i >= 0)
    m_pt->setCurrentIndex(i);
}
void InterfaceWidget::openInterface(int id) {
  m_tabs->setCurrentIndex(0);
  edit(id);
}

void InterfaceWidget::edit(int id) {
  InterfaceRecord r;
  if (id >= 0) {
    QString error;
    r = m_service.get(id, &error);
    if (r.id < 0) {
      QMessageBox::warning(this, "Interface", error);
      return;
    }
  }
  QSqlDatabase db = QSqlDatabase::database(m_connection, false);
  QDialog d(this);
  d.setWindowTitle(id < 0 ? "Nouvelle interface" : "Interface — " + r.code);
  d.resize(900, 700);
  auto *form = new QFormLayout(&d);
  auto *code = new QLineEdit(r.code);
  auto *status = new QComboBox;
  status->addItems({"DRAFT", "IN REVIEW", "APPROVED", "OBSOLETE"});
  status->setCurrentText(r.status);
  auto *p1 = new QComboBox;
  auto *p2 = new QComboBox;
  QSqlQuery q("SELECT ID,NAME FROM PT WHERE COALESCE(ARCHIVED,0)=0 OR ID IN (" +
                  QString::number(r.element1Id) + "," +
                  QString::number(r.element2Id) + ") ORDER BY POSITION,ID",
              db);
  while (q.next()) {
    p1->addItem(q.value(1).toString(), q.value(0));
    p2->addItem(q.value(1).toString(), q.value(0));
  }
  p1->setCurrentIndex(qMax(0, p1->findData(r.element1Id)));
  p2->setCurrentIndex(qMax(0, p2->findData(r.element2Id)));
  auto *description = new QPlainTextEdit(r.description);
  auto *typeList = checkList(
      db, "SELECT ID,LABEL||' ['||CODE||']' FROM INTERFACE_TYPE ORDER BY LABEL",
      r.typeIds, &d);
  auto *reqList = checkList(
      db, "SELECT ID,CODE||' — '||TITLE FROM REQUIREMENT ORDER BY CODE",
      r.requirementIds, &d);
  auto *docList = checkList(db,
                            "SELECT ID,COALESCE(NULLIF(REFERENCE,''),TITLE)||' "
                            "— '||TITLE FROM DOCUMENT ORDER BY TITLE",
                            r.documentIds, &d);
  for (auto *w : {typeList, reqList, docList})
    w->setMaximumHeight(125);
  form->addRow("Code *", code);
  form->addRow("Statut", status);
  form->addRow("Élément PT 1 *", p1);
  form->addRow("Élément PT 2 *", p2);
  form->addRow("Description", description);
  form->addRow("Types", typeList);
  form->addRow("Exigences associées", reqList);
  form->addRow("Documents ICD / chapitre facultatif", docList);
  auto *nav = new QHBoxLayout;
  auto *go1 = new QPushButton("Ouvrir PT 1");
  auto *go2 = new QPushButton("Ouvrir PT 2");
  auto *goReq = new QPushButton("Ouvrir l'exigence cochée");
  auto *goDoc = new QPushButton("Ouvrir le document coché");
  auto *goChanges = new QPushButton("Changements liés");
  nav->addWidget(go1);
  nav->addWidget(go2);
  nav->addWidget(goReq);
  nav->addWidget(goDoc);
  nav->addWidget(goChanges);
  form->addRow(nav);
  connect(go1, &QPushButton::clicked, &d, [this, p1] {
    emit openProductTreeRequested(p1->currentData().toInt());
  });
  connect(go2, &QPushButton::clicked, &d, [this, p2] {
    emit openProductTreeRequested(p2->currentData().toInt());
  });
  connect(goReq, &QPushButton::clicked, &d, [this, reqList] {
    auto ids = checked(reqList);
    if (!ids.isEmpty())
      emit openRequirementRequested(ids.first());
  });
  connect(goDoc, &QPushButton::clicked, &d, [this, docList] {
    auto ids = checked(docList);
    if (!ids.isEmpty())
      emit openDocumentRequested(ids.first());
  });
  connect(goChanges, &QPushButton::clicked, &d,
          [this, id] { if (id >= 0) emit openChangesRequested(id); });
  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  QPushButton *archive = nullptr;
  if (id >= 0) {
    archive = buttons->addButton(r.archived ? "Restaurer" : "Archiver",
                                 QDialogButtonBox::ActionRole);
    connect(archive, &QPushButton::clicked, &d, [this, &d, id, r] {
      auto x = m_service.setArchived(id, !r.archived);
      if (!x.success)
        QMessageBox::warning(&d, "Interface", x.message);
      else
        d.done(2);
    });
  }
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &d, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
  int result = d.exec();
  if (result == QDialog::Accepted) {
    r.code = code->text();
    r.status = status->currentText();
    r.element1Id = p1->currentData().toInt();
    r.element2Id = p2->currentData().toInt();
    r.description = description->toPlainText();
    r.typeIds = checked(typeList);
    r.requirementIds = checked(reqList);
    r.documentIds = checked(docList);
    auto x = m_service.save(r);
    if (!x.success) {
      QMessageBox::warning(this, "Interface", x.message);
      return;
    }
  } else if (result != 2)
    return;
  refresh();
  emit dataChanged();
}
void InterfaceWidget::manageTypes() {
  QDialog d(this);
  d.setWindowTitle("Catalogue des types d'interface");
  auto *l = new QVBoxLayout(&d);
  auto *t = new QTableWidget;
  t->setColumnCount(2);
  t->setHorizontalHeaderLabels({"Code", "Libellé"});
  l->addWidget(t);
  QSqlQuery q("SELECT ID,CODE,LABEL FROM INTERFACE_TYPE ORDER BY LABEL",
              QSqlDatabase::database(m_connection, false));
  while (q.next()) {
    int row = t->rowCount();
    t->insertRow(row);
    for (int c = 0; c < 2; ++c)
      t->setItem(row, c, new QTableWidgetItem(q.value(c + 1).toString()));
    t->item(row, 0)->setData(Qt::UserRole, q.value(0));
  }
  auto *actions = new QHBoxLayout;
  auto *add = new QPushButton("Ajouter");
  auto *remove = new QPushButton("Supprimer");
  actions->addWidget(add);
  actions->addWidget(remove);
  actions->addStretch();
  l->addLayout(actions);
  connect(add, &QPushButton::clicked, &d, [t] {
    int r = t->rowCount();
    t->insertRow(r);
    t->setItem(r, 0, new QTableWidgetItem("NEW_TYPE"));
    t->setItem(r, 1, new QTableWidgetItem("Nouveau type"));
  });
  connect(remove, &QPushButton::clicked, &d, [this, t] {
    int r = t->currentRow();
    if (r < 0)
      return;
    int id = t->item(r, 0)->data(Qt::UserRole).toInt();
    if (id > 0) {
      auto x = m_service.removeType(id);
      if (!x.success) {
        QMessageBox::warning(t, "Types", x.message);
        return;
      }
    }
    t->removeRow(r);
  });
  QDialogButtonBox b(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  l->addWidget(&b);
  connect(&b, &QDialogButtonBox::accepted, &d, &QDialog::accept);
  connect(&b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
  if (d.exec() != QDialog::Accepted)
    return;
  for (int r = 0; r < t->rowCount(); ++r) {
    int id = t->item(r, 0)->data(Qt::UserRole).toInt();
    auto x = m_service.saveType(id > 0 ? id : -1, t->item(r, 0)->text(),
                                t->item(r, 1)->text());
    if (!x.success) {
      QMessageBox::warning(this, "Types", x.message);
      break;
    }
  }
  refresh();
}
