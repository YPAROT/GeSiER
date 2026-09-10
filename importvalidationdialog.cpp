#include "importvalidationdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
QString lineSummary(QList<int> lines) {
  std::sort(lines.begin(), lines.end());
  QStringList ranges;
  for (int index = 0; index < lines.size();) {
    int end = index;
    while (end + 1 < lines.size() && lines[end + 1] == lines[end] + 1)
      ++end;
    ranges << (end == index ? QString::number(lines[index])
                            : QString("%1–%2").arg(lines[index]).arg(lines[end]));
    index = end + 1;
  }
  return ranges.join(", ");
}
}

ImportValidationDialog::ImportValidationDialog(
    const QList<ImportConflict> &conflicts, QWidget *parent)
    : QDialog(parent), m_table(new QTableWidget(conflicts.size(), 5, this)) {
  setWindowTitle("Résolution des correspondances d'import");
  resize(1000, qMin(700, 220 + conflicts.size() * 42));
  m_table->setHorizontalHeaderLabels(
      {"Catégorie", "Valeur Excel", "Occurrences", "Lignes", "Résolution"});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
  m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
  for (int row = 0; row < conflicts.size(); ++row) {
    const ImportConflict &conflict = conflicts[row];
    m_table->setItem(row, 0, new QTableWidgetItem(conflict.category));
    m_table->setItem(row, 1, new QTableWidgetItem(conflict.sourceValue));
    m_table->setItem(row, 2,
                     new QTableWidgetItem(QString::number(conflict.occurrences)));
    m_table->setItem(
        row, 3,
        new QTableWidgetItem(lineSummary(conflict.lines.values())));
    auto *resolution = new QComboBox;
    resolution->setProperty("conflictKey", conflict.key);
    resolution->addItem("— Ignorer cette valeur —", -1);
    resolution->addItem("Rejeter les lignes concernées", -2);
    if (conflict.category == "Configuration inconnue")
      resolution->addItem("Créer une nouvelle configuration", -3);
    for (const auto &choice : conflict.choices)
      resolution->addItem(choice.second, choice.first);
    m_table->setCellWidget(row, 4, resolution);
  }
  auto *summary = new QLabel(
      QString("%1 correspondance(s) à résoudre. Chaque décision s'applique à "
              "toutes les occurrences de la même valeur.")
          .arg(conflicts.size()));
  summary->setWordWrap(true);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel);
  buttons->button(QDialogButtonBox::Ok)->setText("Appliquer les décisions");
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(summary);
  layout->addWidget(m_table);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QMap<QString, int> ImportValidationDialog::decisions() const {
  QMap<QString, int> result;
  for (int row = 0; row < m_table->rowCount(); ++row) {
    auto *combo = qobject_cast<QComboBox *>(m_table->cellWidget(row, 4));
    if (combo)
      result[combo->property("conflictKey").toString()] =
          combo->currentData().toInt();
  }
  return result;
}

bool ImportValidationDialog::askToImportValidRows(const QStringList &errors,
                                                  int validRows,
                                                  QWidget *parent) {
  QMap<QString, ImportConflict> groups;
  const QRegularExpression expression("^Ligne (\\d+) : (.*)$");
  for (const QString &error : errors) {
    const QRegularExpressionMatch match = expression.match(error);
    const QString message = match.hasMatch() ? match.captured(2) : error;
    ImportConflict &group = groups[message];
    group.category = "Erreur bloquante";
    group.sourceValue = message;
    ++group.occurrences;
    if (match.hasMatch())
      group.lines.insert(match.captured(1).toInt());
  }
  QDialog dialog(parent);
  dialog.setWindowTitle("Prévalidation de l'import");
  dialog.resize(900, qMin(700, 220 + groups.size() * 42));
  auto *table = new QTableWidget(groups.size(), 3, &dialog);
  table->setHorizontalHeaderLabels({"Erreur", "Occurrences", "Lignes"});
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  int row = 0;
  for (const ImportConflict &group : groups) {
    table->setItem(row, 0, new QTableWidgetItem(group.sourceValue));
    table->setItem(row, 1, new QTableWidgetItem(QString::number(group.occurrences)));
    table->setItem(row, 2,
                   new QTableWidgetItem(lineSummary(group.lines.values())));
    ++row;
  }
  auto *buttons = new QDialogButtonBox;
  auto *cancelAll = buttons->addButton("Annuler tout l'import",
                                       QDialogButtonBox::RejectRole);
  auto *continueValid = buttons->addButton(
      QString("Importer les %1 ligne(s) valide(s)").arg(validRows),
      QDialogButtonBox::AcceptRole);
  continueValid->setEnabled(validRows > 0);
  auto *layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel(
      QString("%1 anomalie(s) ont été détectées. Aucune modification n'a "
              "encore été effectuée.")
          .arg(errors.size())));
  layout->addWidget(table);
  layout->addWidget(buttons);
  QObject::connect(cancelAll, &QPushButton::clicked, &dialog,
                   &QDialog::reject);
  QObject::connect(continueValid, &QPushButton::clicked, &dialog,
                   &QDialog::accept);
  return dialog.exec() == QDialog::Accepted;
}

ImportDuplicateDialog::ImportDuplicateDialog(
    const QList<ImportDuplicate> &duplicates, const QString &defaultAction,
    QWidget *parent)
    : QDialog(parent), m_table(new QTableWidget(duplicates.size(), 6, this)) {
  setWindowTitle("Exigences déjà présentes");
  resize(1200, qMin(750, 220 + duplicates.size() * 46));
  m_table->setHorizontalHeaderLabels(
      {"Code", "Titre existant", "Titre importé", "Descriptif existant",
       "Descriptif importé", "Décision"});
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  for (int row = 0; row < duplicates.size(); ++row) {
    const ImportDuplicate &duplicate = duplicates[row];
    m_table->setItem(row, 0, new QTableWidgetItem(duplicate.code));
    m_table->setItem(row, 1, new QTableWidgetItem(duplicate.existingTitle));
    m_table->setItem(row, 2, new QTableWidgetItem(duplicate.importedTitle));
    m_table->setItem(row, 3,
                     new QTableWidgetItem(duplicate.existingDescription));
    m_table->setItem(row, 4,
                     new QTableWidgetItem(duplicate.importedDescription));
    auto *decision = new QComboBox;
    decision->setProperty("code", duplicate.code);
    decision->addItem("Même exigence — conserver et rattacher", "keep");
    decision->addItem("Même exigence — mettre à jour et rattacher", "update");
    decision->addItem("Ignorer totalement", "skip");
    decision->addItem("Rejeter la ligne", "reject");
    const int selected = decision->findData(defaultAction == "update" ? "update" :
                                            defaultAction == "skip" ? "keep" : "reject");
    decision->setCurrentIndex(qMax(0, selected));
    m_table->setCellWidget(row, 5, decision);
  }
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                       QDialogButtonBox::Cancel);
  auto *layout = new QVBoxLayout(this);
  layout->addWidget(new QLabel("Confirmez qu'il s'agit bien des mêmes exigences avant de les rattacher à la spécification."));
  layout->addWidget(m_table);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QMap<QString, QString> ImportDuplicateDialog::decisions() const {
  QMap<QString, QString> result;
  for (int row = 0; row < m_table->rowCount(); ++row) {
    auto *combo = qobject_cast<QComboBox *>(m_table->cellWidget(row, 5));
    if (combo)
      result[combo->property("code").toString()] = combo->currentData().toString();
  }
  return result;
}
