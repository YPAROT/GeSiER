#include "changewidget.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUrl>
#include <QtWidgets>

namespace {
void combo(QComboBox *c, QSqlDatabase db, const QString &s,
           const QString &all) {
  QVariant old = c->currentData();
  c->blockSignals(true);
  c->clear();
  c->addItem(all, -1);
  QSqlQuery q(s, db);
  while (q.next())
    c->addItem(q.value(1).toString(), q.value(0));
  int i = c->findData(old);
  c->setCurrentIndex(i < 0 ? 0 : i);
  c->blockSignals(false);
}
QListWidget *choices(QSqlDatabase db, const QString &sql, const QString &type,
                     const QList<ChangeLink> &selected, QWidget *p) {
  auto *w = new QListWidget(p);
  QSet<int> ids;
  for (auto &l : selected)
    if (l.objectType == type)
      ids << l.objectId;
  QSqlQuery q(sql, db);
  while (q.next()) {
    auto *i = new QListWidgetItem(q.value(1).toString(), w);
    i->setData(Qt::UserRole, q.value(0));
    i->setFlags(i->flags() | Qt::ItemIsUserCheckable);
    i->setCheckState(ids.contains(q.value(0).toInt()) ? Qt::Checked
                                                      : Qt::Unchecked);
  }
  return w;
}
void collect(QListWidget *w, const QString &t, QList<ChangeLink> &out) {
  for (int i = 0; i < w->count(); ++i)
    if (w->item(i)->checkState() == Qt::Checked)
      out << ChangeLink{t, w->item(i)->text(),
                        w->item(i)->data(Qt::UserRole).toInt()};
}
} // namespace
ChangeWidget::ChangeWidget(QWidget *p) : QWidget(p) {
  m_search = new QLineEdit;
  m_search->setPlaceholderText(
      "Identifiant, description, décision, référence…");
  m_type = new QComboBox;
  m_status = new QComboBox;
  m_archived = new QCheckBox("Inclure archivés");
  m_incomplete = new QCheckBox("Décision incomplète");
  auto *add = new QPushButton("Nouveau");
  auto *editButton = new QPushButton("Ouvrir");
  auto *types = new QPushButton("Types");
  auto *statuses = new QPushButton("Statuts");
  auto *exportButton = new QPushButton("Exporter XLSX");
  auto *clearScope = new QPushButton("Tous les objets");
  auto *bar = new QHBoxLayout;
  bar->addWidget(m_search, 2);
  bar->addWidget(m_type);
  bar->addWidget(m_status);
  bar->addWidget(m_incomplete);
  bar->addWidget(m_archived);
  bar->addWidget(clearScope);
  bar->addWidget(add);
  bar->addWidget(editButton);
  bar->addWidget(types);
  bar->addWidget(statuses);
  bar->addWidget(exportButton);
  m_table = new QTableWidget;
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->setSortingEnabled(true);
  m_details = new QTextBrowser;
  m_details->setOpenExternalLinks(false);
  m_openLink = new QPushButton("Ouvrir le lien externe");
  m_summary = new QLabel;
  auto *right = new QWidget;
  auto *rl = new QVBoxLayout(right);
  rl->addWidget(new QLabel("Fiche et historique"));
  rl->addWidget(m_details, 1);
  rl->addWidget(m_openLink);
  auto *split = new QSplitter;
  split->addWidget(m_table);
  split->addWidget(right);
  split->setStretchFactor(0, 3);
  split->setStretchFactor(1, 2);
  auto *root = new QVBoxLayout(this);
  root->addLayout(bar);
  root->addWidget(m_summary);
  root->addWidget(split, 1);
  for (auto *o : {m_search})
    connect(o, &QLineEdit::textChanged, this, &ChangeWidget::refresh);
  for (auto *c : {m_type, m_status})
    connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ChangeWidget::refresh);
  connect(m_archived, &QCheckBox::toggled, this, &ChangeWidget::refresh);
  connect(m_incomplete, &QCheckBox::toggled, this, &ChangeWidget::refresh);
  connect(clearScope, &QPushButton::clicked, this, [this] {
    m_objectType.clear();
    m_objectId = -1;
    refresh();
  });
  connect(add, &QPushButton::clicked, this, [this] { edit(); });
  connect(editButton, &QPushButton::clicked, this, [this] {
    if (m_table->currentRow() >= 0)
      edit(m_table->item(m_table->currentRow(), 0)->data(Qt::UserRole).toInt());
  });
  connect(m_table, &QTableWidget::doubleClicked, this, [this] {
    if (m_table->currentRow() >= 0)
      edit(m_table->item(m_table->currentRow(), 0)->data(Qt::UserRole).toInt());
  });
  connect(m_table, &QTableWidget::itemSelectionChanged, this,
          &ChangeWidget::showDetails);
  connect(types, &QPushButton::clicked, this,
          [this] { catalog("CHANGE_TYPE"); });
  connect(statuses, &QPushButton::clicked, this,
          [this] { catalog("CHANGE_STATUS"); });
  connect(exportButton, &QPushButton::clicked, this, [this] {
    QString p = QFileDialog::getSaveFileName(this, "Exporter les changements",
                                             {}, "Excel (*.xlsx)");
    if (p.isEmpty())
      return;
    if (!p.endsWith(".xlsx", Qt::CaseInsensitive))
      p += ".xlsx";
    auto r = m_service.exportXlsx(p, filter());
    QMessageBox::information(this, "Export XLSX", r.message);
  });
  connect(m_openLink, &QPushButton::clicked, this, [this] {
    int row = m_table->currentRow();
    if (row >= 0) {
      auto record =
          m_service.get(m_table->item(row, 0)->data(Qt::UserRole).toInt());
      QDesktopServices::openUrl(QUrl::fromUserInput(record.externalLink));
    }
  });
}
void ChangeWidget::setConnectionName(const QString &n) {
  m_connection = n;
  m_service.setConnectionName(n);
  refresh();
}
ChangeFilter ChangeWidget::filter() const {
  ChangeFilter f;
  f.text = m_search->text();
  f.typeId = m_type->currentData().toInt();
  f.statusId = m_status->currentData().toInt();
  f.includeArchived = m_archived->isChecked();
  f.onlyIncomplete = m_incomplete->isChecked();
  f.objectType = m_objectType;
  f.objectId = m_objectId;
  return f;
}
void ChangeWidget::refresh() {
  auto db = QSqlDatabase::contains(m_connection)
                ? QSqlDatabase::database(m_connection, false)
                : QSqlDatabase();
  if (!db.isValid() || !db.isOpen()) {
    m_table->setRowCount(0);
    m_details->clear();
    m_summary->setText("Aucun projet ouvert.");
    return;
  }
  combo(m_type, db,
        "SELECT ID,LABEL FROM CHANGE_TYPE WHERE ACTIVE=1 ORDER BY LABEL",
        "Tous types");
  combo(m_status, db,
        "SELECT ID,LABEL FROM CHANGE_STATUS ORDER BY POSITION,LABEL",
        "Tous statuts");
  QString e;
  m_rows = m_service.find(filter(), &e);
  m_table->setSortingEnabled(false);
  m_table->setColumnCount(8);
  m_table->setHorizontalHeaderLabels({"Identifiant", "Type", "Statut",
                                      "Décision", "Ouverture", "Clôture",
                                      "Référence", "État"});
  m_table->setRowCount(m_rows.size());
  for (int r = 0; r < m_rows.size(); ++r) {
    auto &x = m_rows[r];
    QStringList v{x.code,
                  x.typeLabel,
                  x.statusLabel,
                  x.decision,
                  x.openedAt,
                  x.closedAt,
                  x.externalReference,
                  x.archived ? "Archivé" : "Actif"};
    for (int c = 0; c < v.size(); ++c)
      m_table->setItem(r, c, new QTableWidgetItem(v[c]));
    m_table->item(r, 0)->setData(Qt::UserRole, x.id);
    if (!x.finalStatus || x.decision.trimmed().isEmpty())
      for (int c = 0; c < m_table->columnCount(); ++c)
        m_table->item(r, c)->setBackground(QColor("#fff1cc"));
  }
  m_table->setSortingEnabled(true);
  m_table->horizontalHeader()->setStretchLastSection(true);
  auto cov = m_service.coverage(filter());
  m_summary->setText(
      e.isEmpty()
          ? QString("%1 objet(s)%4 — décision + statut final : %2 / %1 (%3 %)")
                .arg(cov.total)
                .arg(cov.complete)
                .arg(cov.total ? 100.0 * cov.complete / cov.total : 0., 0, 'f',
                     2)
                .arg(m_objectId >= 0
                         ? QString(" liés à %1 #%2").arg(m_objectType).arg(m_objectId)
                         : QString())
          : e);
  showDetails();
}
void ChangeWidget::showDetails() {
  int row = m_table->currentRow();
  if (row < 0) {
    m_details->clear();
    m_openLink->setEnabled(false);
    return;
  }
  auto r =
      m_service.get(m_table->item(row, 0)->data(Qt::UserRole).toInt());
  QString h = "<h3>" + r.code.toHtmlEscaped() + "</h3><p><b>" +
              r.typeLabel.toHtmlEscaped() + " — " +
              r.statusLabel.toHtmlEscaped() + "</b></p><p>" +
              r.description.toHtmlEscaped().replace("\n", "<br>") +
              "</p><p><b>Décision :</b> " + r.decision.toHtmlEscaped() +
      "</p><p><b>Exigences concernées</b></p><ul>";
  for (auto &l : r.links)
    h += "<li><a href=\"" + l.objectType + ":" + QString::number(l.objectId) +
         "\">" + l.objectType + " — " + l.label.toHtmlEscaped() + "</a></li>";
  h += "</ul><p><b>Historique</b></p><ul>";
  QSqlQuery q(QSqlDatabase::database(m_connection, false));
  q.prepare("SELECT EVENT_TIME,EVENT_TYPE FROM EVENT_LOG WHERE "
            "OBJECT_TYPE='CHANGE' AND OBJECT_ID=? ORDER BY ID DESC LIMIT 20");
  q.addBindValue(r.id);
  q.exec();
  while (q.next())
    h += "<li>" + q.value(0).toString().toHtmlEscaped() + " — " +
         q.value(1).toString().toHtmlEscaped() + "</li>";
  h += "</ul>";
  m_details->setHtml(h);
  m_openLink->setEnabled(!r.externalLink.trimmed().isEmpty());
  disconnect(m_details, &QTextBrowser::anchorClicked, nullptr, nullptr);
  connect(m_details, &QTextBrowser::anchorClicked, this, [this](const QUrl &u) {
    QStringList p = u.toString().split(':');
    if (p.size() != 2)
      return;
    int id = p[1].toInt();
    if (p[0] == "REQUIREMENT")
      emit openRequirementRequested(id);
    else if (p[0] == "PT")
      emit openProductTreeRequested(id);
    else if (p[0] == "CONFIGURATION")
      emit openConfigurationRequested(id);
    else if (p[0] == "INTERFACE")
      emit openInterfaceRequested(id);
    else if (p[0] == "DOCUMENT")
      emit openDocumentRequested(id);
  });
}
void ChangeWidget::edit(int id) {
  ChangeRecord r;
  if (id >= 0)
    r = m_service.get(id);
  auto db = QSqlDatabase::database(m_connection, false);
  QDialog d(this);
  d.setWindowTitle(id < 0 ? "Nouveau changement" : "Changement — " + r.code);
  d.resize(950, 820);
  auto *f = new QFormLayout(&d);
  auto *code = new QLineEdit(r.code);
  auto *type = new QComboBox;
  combo(type, db,
        "SELECT ID,LABEL FROM CHANGE_TYPE WHERE ACTIVE=1 ORDER BY LABEL", "");
  type->setCurrentIndex(qMax(0, type->findData(r.typeId)));
  auto *status = new QComboBox;
  combo(status, db,
        "SELECT ID,LABEL FROM CHANGE_STATUS ORDER BY POSITION,LABEL", "");
  status->setCurrentIndex(qMax(0, status->findData(r.statusId)));
  auto *desc = new QPlainTextEdit(r.description);
  auto *decision = new QPlainTextEdit(r.decision);
  auto *opened =
      new QDateTimeEdit(QDateTime::fromString(r.openedAt, Qt::ISODate));
  opened->setCalendarPopup(true);
  opened->setDisplayFormat("yyyy-MM-dd HH:mm");
  if (!opened->dateTime().isValid())
    opened->setDateTime(QDateTime::currentDateTime());
  auto *closed =
      new QDateTimeEdit(QDateTime::fromString(r.closedAt, Qt::ISODate));
  closed->setCalendarPopup(true);
  closed->setDisplayFormat("yyyy-MM-dd HH:mm");
  if (!closed->dateTime().isValid())
    closed->setDateTime(QDateTime::currentDateTime());
  auto *ref = new QLineEdit(r.externalReference);
  auto *url = new QLineEdit(r.externalLink);
  auto *req =
      choices(db, "SELECT ID,CODE||' — '||TITLE FROM REQUIREMENT ORDER BY CODE",
              "REQUIREMENT", r.links, &d);
  req->setMaximumHeight(190);
  f->addRow("Identifiant *", code);
  f->addRow("Type *", type);
  f->addRow("Statut *", status);
  f->addRow("Description", desc);
  f->addRow("Décision", decision);
  f->addRow("Ouverture", opened);
  f->addRow("Clôture", closed);
  f->addRow("Référence externe", ref);
  f->addRow("Lien externe", url);
  f->addRow("Exigences concernées", req);
  auto *b =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  if (id >= 0) {
    auto *a = b->addButton(r.archived ? "Restaurer" : "Archiver",
                           QDialogButtonBox::ActionRole);
    connect(a, &QPushButton::clicked, &d, [this, &d, id, r] {
      auto x = m_service.setArchived(id, !r.archived);
      if (x.success)
        d.done(2);
      else
        QMessageBox::warning(&d, "Changement", x.message);
    });
  }
  f->addRow(b);
  connect(b, &QDialogButtonBox::accepted, &d, &QDialog::accept);
  connect(b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
  int result = d.exec();
  if (result == QDialog::Accepted) {
    r.code = code->text();
    r.typeId = type->currentData().toInt();
    r.statusId = status->currentData().toInt();
    r.description = desc->toPlainText();
    r.decision = decision->toPlainText();
    r.openedAt = opened->dateTime().toString(Qt::ISODate);
    r.closedAt = closed->dateTime().toString(Qt::ISODate);
    r.externalReference = ref->text();
    r.externalLink = url->text();
    QList<ChangeLink> legacyLinks;
    for (const auto &link : r.links)
      if (link.objectType != "REQUIREMENT")
        legacyLinks << link;
    r.links = legacyLinks;
    collect(req, "REQUIREMENT", r.links);
    auto x = m_service.save(r);
    if (!x.success) {
      QMessageBox::warning(this, "Changement", x.message);
      return;
    }
  } else if (result != 2)
    return;
  refresh();
  emit dataChanged();
}
void ChangeWidget::catalog(const QString &table) {
  bool statuses = table == "CHANGE_STATUS";
  QDialog d(this);
  d.setWindowTitle(statuses ? "Catalogue des statuts" : "Catalogue des types");
  auto *l = new QVBoxLayout(&d);
  auto *t = new QTableWidget;
  t->setColumnCount(statuses ? 4 : 3);
  t->setHorizontalHeaderLabels(
      statuses ? QStringList{"Code", "Libellé", "Final", "Position"}
               : QStringList{"Code", "Libellé", "Actif"});
  QSqlQuery q("SELECT ID,CODE,LABEL," +
                  QString(statuses ? "IS_FINAL,POSITION" : "ACTIVE") +
                  " FROM " + table + " ORDER BY " +
                  QString(statuses ? "POSITION,LABEL" : "LABEL"),
              QSqlDatabase::database(m_connection, false));
  while (q.next()) {
    int r = t->rowCount();
    t->insertRow(r);
    for (int c = 0; c < t->columnCount(); ++c)
      t->setItem(r, c, new QTableWidgetItem(q.value(c + 1).toString()));
    t->item(r, 0)->setData(Qt::UserRole, q.value(0));
  }
  l->addWidget(t);
  auto *add = new QPushButton("Ajouter");
  l->addWidget(add);
  connect(add, &QPushButton::clicked, &d, [t, statuses] {
    int r = t->rowCount();
    t->insertRow(r);
    QStringList v =
        statuses ? QStringList{"NEW_STATUS", "Nouveau statut", "0", "100"}
                 : QStringList{"NEW_TYPE", "Nouveau type", "1"};
    for (int c = 0; c < v.size(); ++c)
      t->setItem(r, c, new QTableWidgetItem(v[c]));
  });
  auto *b =
      new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
  l->addWidget(b);
  connect(b, &QDialogButtonBox::accepted, &d, &QDialog::accept);
  connect(b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
  if (d.exec() != QDialog::Accepted)
    return;
  for (int row = 0; row < t->rowCount(); ++row) {
    int id = t->item(row, 0)->data(Qt::UserRole).toInt();
    RequirementResult x =
        statuses ? m_service.saveStatus(id ? id : -1, t->item(row, 0)->text(),
                                        t->item(row, 1)->text(),
                                        t->item(row, 2)->text().toInt(),
                                        t->item(row, 3)->text().toInt())
                 : m_service.saveType(id ? id : -1, t->item(row, 0)->text(),
                                      t->item(row, 1)->text(),
                                      t->item(row, 2)->text().toInt());
    if (!x.success) {
      QMessageBox::warning(this, "Catalogue", x.message);
      break;
    }
  }
  refresh();
}
void ChangeWidget::applyObjectFilter(const QString &type, int id) {
  m_objectType = type;
  m_objectId = id;
  m_search->clear();
  refresh();
}
void ChangeWidget::openChange(int id) {
  for (int r = 0; r < m_table->rowCount(); ++r)
    if (m_table->item(r, 0)->data(Qt::UserRole).toInt() == id) {
      m_table->selectRow(r);
      break;
    }
}
