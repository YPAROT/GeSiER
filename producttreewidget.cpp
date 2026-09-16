#include "producttreewidget.h"
#include "producttreemodel.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QtWidgets>
#include <functional>
ProductTreeWidget::ProductTreeWidget(QWidget *p) : QWidget(p) {
  m_search = new QLineEdit;
  m_search->setPlaceholderText("Rechercher…");
  m_root = new QPushButton("Ajouter la racine");
  m_child = new QPushButton("Ajouter un enfant");
  m_remove = new QPushButton("Supprimer");
  m_archive = new QPushButton("Archiver / restaurer");
  m_showArchived = new QCheckBox("Afficher les archivés");
  auto tools = new QHBoxLayout;
  for (auto w : {m_root, m_child, m_remove, m_archive})
    tools->addWidget(w);
  tools->addWidget(m_showArchived);
  tools->addStretch();
  tools->addWidget(m_search);
  m_model = new ProductTreeModel(this);
  m_tree = new QTreeView;
  m_tree->setModel(m_model);
  m_tree->setDragDropMode(QAbstractItemView::InternalMove);
  m_tree->setDefaultDropAction(Qt::MoveAction);
  m_tree->setEditTriggers(QAbstractItemView::SelectedClicked |
                          QAbstractItemView::EditKeyPressed);
  m_segment = new QLineEdit;
  m_code = new QLineEdit;
  m_code->setReadOnly(true);
  m_description = new QTextEdit;
  auto saveButton = new QPushButton("Enregistrer les propriétés");
  auto form = new QFormLayout;
  form->addRow("Segment local", m_segment);
  form->addRow("Code complet", m_code);
  form->addRow("Description", m_description);
  form->addRow(saveButton);
  m_links = new QTableWidget(0, 2);
  m_links->setHorizontalHeaderLabels({"Objet lié", "Référence"});
  m_links->horizontalHeader()->setStretchLastSection(true);
  m_links->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_metrics = new QTableWidget(3, 2);
  m_metrics->setVerticalHeaderLabels(
      {"Exigences", "Interfaces", "Changements"});
  m_metrics->setHorizontalHeaderLabels({"Directs", "Branche"});
  auto right = new QWidget;
  auto rv = new QVBoxLayout(right);
  rv->addLayout(form);
  rv->addWidget(new QLabel("Éléments liés (double-cliquer pour ouvrir)"));
  rv->addWidget(m_links);
  rv->addWidget(new QLabel("Métriques"));
  rv->addWidget(m_metrics);
  auto split = new QSplitter;
  split->addWidget(m_tree);
  split->addWidget(right);
  split->setStretchFactor(0, 1);
  split->setStretchFactor(1, 1);
  split->restoreState(QSettings().value("ProductTree/splitter").toByteArray());
  auto layout = new QVBoxLayout(this);
  layout->addLayout(tools);
  layout->addWidget(split);
  connect(split, &QSplitter::splitterMoved, this, [split] {
    QSettings().setValue("ProductTree/splitter", split->saveState());
  });
  connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this] { selectNode(); });
  connect(m_root, &QPushButton::clicked, this, &ProductTreeWidget::addRoot);
  connect(m_child, &QPushButton::clicked, this, &ProductTreeWidget::addChild);
  connect(m_remove, &QPushButton::clicked, this, &ProductTreeWidget::remove);
  connect(m_archive, &QPushButton::clicked, this, &ProductTreeWidget::archive);
  connect(saveButton, &QPushButton::clicked, this, &ProductTreeWidget::save);
  connect(m_showArchived, &QCheckBox::toggled, m_model,
          &ProductTreeModel::setShowArchived);
  connect(m_model, &ProductTreeModel::moveRequested, this,
          &ProductTreeWidget::moveRequested);
  connect(m_model, &ProductTreeModel::renameRequested, this,
          &ProductTreeWidget::renameRequested);
  connect(m_search, &QLineEdit::textChanged, this, [this](const QString &t) {
    std::function<bool(QModelIndex)> visit = [&](QModelIndex i) {
      bool match =
          t.isEmpty() || i.data().toString().contains(t, Qt::CaseInsensitive) ||
          i.siblingAtColumn(1).data().toString().contains(t,
                                                          Qt::CaseInsensitive);
      for (int x = 0; x < m_model->rowCount(i); ++x)
        match = visit(m_model->index(x, 0, i)) || match;
      m_tree->setRowHidden(i.row(), i.parent(), !match);
      return match;
    };
    for (int r = 0; r < m_model->rowCount(); ++r)
      visit(m_model->index(r, 0));
  });
  connect(m_links, &QTableWidget::cellDoubleClicked, this, [this](int r, int) {
    QString type = m_links->item(r, 0)->data(Qt::UserRole).toString();
    if (type == "REQUIREMENT")
      emit openRequirementsForPt(selectedId());
    else if (type == "INTERFACE")
      emit openInterfacesForPt(selectedId());
    else
      emit openChangesForPt(selectedId());
  });
}
void ProductTreeWidget::setConnectionName(const QString &n) {
  m_connection = n;
  m_service.setConnectionName(n);
  m_model->setConnectionName(n);
  refresh();
}
int ProductTreeWidget::selectedId() const {
  return m_model->id(m_tree->currentIndex());
}
void ProductTreeWidget::refresh() {
  int id = selectedId();
  m_model->reload();
  m_tree->expandAll();
  if (id >= 0)
    m_tree->setCurrentIndex(m_model->indexForId(id));
  const QSqlDatabase db = QSqlDatabase::database(m_connection, false);
  if (m_connection.isEmpty() || !db.isValid() || !db.isOpen()) {
    m_root->setEnabled(false);
    m_segment->clear();
    m_description->clear();
    m_code->clear();
    m_links->setRowCount(0);
    m_metrics->clearContents();
    return;
  }
  QSqlQuery q("SELECT COUNT(*) FROM PT WHERE PARENT IS NULL AND ARCHIVED=0",
              db);
  m_root->setEnabled(q.next() && q.value(0).toInt() == 0);
}
void ProductTreeWidget::addRoot() {
  bool ok;
  QString s = QInputDialog::getText(this, "Racine", "Segment",
                                    QLineEdit::Normal, {}, &ok);
  if (ok)
    showResult(m_service.addNode(-1, s));
}
void ProductTreeWidget::addChild() {
  if (selectedId() < 0)
    return;
  bool ok;
  QString s = QInputDialog::getText(this, "Nouvel élément", "Segment",
                                    QLineEdit::Normal, {}, &ok);
  if (ok)
    showResult(m_service.addNode(selectedId(), s));
}
void ProductTreeWidget::save() {
  if (selectedId() >= 0)
    showResult(m_service.updateNode(selectedId(), m_segment->text(),
                                    m_description->toPlainText()));
}
void ProductTreeWidget::remove() {
  if (selectedId() >= 0 &&
      QMessageBox::question(this, "Suppression",
                            "Supprimer cette branche inutilisée ?") ==
          QMessageBox::Yes)
    showResult(m_service.removeNode(selectedId()));
}
void ProductTreeWidget::archive() {
  if (selectedId() < 0)
    return;
  QSqlQuery q(QSqlDatabase::database(m_connection, false));
  q.prepare("SELECT ARCHIVED FROM PT WHERE ID=?");
  q.addBindValue(selectedId());
  q.exec();
  q.next();
  showResult(m_service.setArchived(selectedId(), !q.value(0).toBool()));
}
void ProductTreeWidget::showResult(const ProductTreeResult &r) {
  if (!r.success)
    QMessageBox::warning(this, "Product Tree",
                         r.message + (r.blockers.isEmpty()
                                          ? ""
                                          : "\n\n" + r.blockers.join("\n")));
  else {
    refresh();
    emit dataChanged();
  }
}
void ProductTreeWidget::selectNode() {
  int id = selectedId();
  m_links->setRowCount(0);
  if (id < 0) {
    m_segment->clear();
    m_description->clear();
    m_code->clear();
    m_metrics->clearContents();
    return;
  }
  QSqlQuery q(QSqlDatabase::database(m_connection, false));
  q.prepare("SELECT SEGMENT,DESCRIPTION FROM PT WHERE ID=?");
  q.addBindValue(id);
  q.exec();
  q.next();
  m_segment->setText(q.value(0).toString());
  m_description->setPlainText(q.value(1).toString());
  m_code->setText(m_service.fullCode(id));
  populateLinks(id);
  populateMetrics(id);
}
void ProductTreeWidget::populateLinks(int id) {
  QSqlQuery q(QSqlDatabase::database(m_connection, false));
  q.prepare("SELECT 'REQUIREMENT',R.CODE||' — '||R.TITLE FROM REQUIREMENT_PT X "
            "JOIN REQUIREMENT R ON R.ID=X.REQ_ID WHERE X.PT_ID=? UNION ALL "
            "SELECT 'INTERFACE',COALESCE(I.CODE,'IF-'||I.ID)||' — "
            "'||COALESCE(I.DESCRIPTION,'') FROM INTERFACE I WHERE I.ELEMENT1=? "
            "OR I.ELEMENT2=? UNION ALL SELECT 'CHANGE',C.CODE||' — "
            "'||COALESCE(C.DESCRIPTION,'') FROM CHANGE_LINK L JOIN CHANGE_ITEM "
            "C ON C.ID=L.CHANGE_ID WHERE L.OBJECT_TYPE='PT' AND L.OBJECT_ID=?");
  for (int i = 0; i < 4; i++)
    q.addBindValue(id);
  q.exec();
  while (q.next()) {
    int r = m_links->rowCount();
    m_links->insertRow(r);
    auto kind = new QTableWidgetItem(
        q.value(0).toString() == "REQUIREMENT" ? "Exigence"
        : q.value(0).toString() == "INTERFACE" ? "Interface"
                                               : "Changement");
    kind->setData(Qt::UserRole, q.value(0));
    m_links->setItem(r, 0, kind);
    m_links->setItem(r, 1, new QTableWidgetItem(q.value(1).toString()));
  }
}
void ProductTreeWidget::populateMetrics(int id) {
  QStringList direct = {
      "SELECT COUNT(*) FROM REQUIREMENT_PT WHERE PT_ID=?",
      "SELECT COUNT(*) FROM INTERFACE WHERE ELEMENT1=? OR ELEMENT2=?",
      "SELECT COUNT(*) FROM CHANGE_LINK WHERE OBJECT_TYPE='PT' AND "
      "OBJECT_ID=?"};
  QStringList branch = {
      "SELECT COUNT(*) FROM REQUIREMENT_PT WHERE PT_ID IN(SELECT ID FROM D)",
      "SELECT COUNT(*) FROM INTERFACE WHERE ELEMENT1 IN(SELECT ID FROM D) OR "
      "ELEMENT2 IN(SELECT ID FROM D)",
      "SELECT COUNT(*) FROM CHANGE_LINK WHERE OBJECT_TYPE='PT' AND OBJECT_ID "
      "IN(SELECT ID FROM D)"};
  for (int r = 0; r < 3; r++) {
    QSqlQuery a(QSqlDatabase::database(m_connection, false));
    a.prepare(direct[r]);
    a.addBindValue(id);
    if (r == 1)
      a.addBindValue(id);
    a.exec();
    a.next();
    m_metrics->setItem(r, 0, new QTableWidgetItem(a.value(0).toString()));
    QSqlQuery b(QSqlDatabase::database(m_connection, false));
    b.prepare("WITH RECURSIVE D(ID) AS(SELECT ? UNION ALL SELECT P.ID FROM PT "
              "P JOIN D ON P.PARENT=D.ID) " +
              branch[r]);
    b.addBindValue(id);
    b.exec();
    b.next();
    m_metrics->setItem(r, 1, new QTableWidgetItem(b.value(0).toString()));
  }
}
void ProductTreeWidget::moveRequested(int id, int parent, int pos) {
  QString text = QString("Ancien code : %1\nNouveau parent : %2\n%3 élément(s) "
                         "concerné(s).\n\nConfirmer le déplacement ?")
                     .arg(m_service.fullCode(id),
                          parent < 0 ? "(racine)" : m_service.fullCode(parent))
                     .arg(m_service.subtreeCodes(id).size());
  if (QMessageBox::question(this, "Aperçu du déplacement", text) ==
      QMessageBox::Yes)
    showResult(m_service.moveNode(id, parent, pos));
  else
    refresh();
}
void ProductTreeWidget::renameRequested(int id, const QString &s) {
  QSqlQuery q(QSqlDatabase::database(m_connection, false));
  q.prepare("SELECT DESCRIPTION FROM PT WHERE ID=?");
  q.addBindValue(id);
  q.exec();
  q.next();
  showResult(m_service.updateNode(id, s, q.value(0).toString()));
}
