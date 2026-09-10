#ifndef IMPORTVALIDATIONDIALOG_H
#define IMPORTVALIDATIONDIALOG_H

#include <QDialog>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>

struct ImportConflict {
  QString key;
  QString category;
  QString sourceValue;
  int occurrences = 0;
  QSet<int> lines;
  QList<QPair<int, QString>> choices;
};

class QTableWidget;

class ImportValidationDialog : public QDialog {
  Q_OBJECT
public:
  explicit ImportValidationDialog(const QList<ImportConflict> &conflicts,
                                  QWidget *parent = nullptr);
  QMap<QString, int> decisions() const;

  static bool askToImportValidRows(const QStringList &errors, int validRows,
                                   QWidget *parent = nullptr);

private:
  QTableWidget *m_table;
};

struct ImportDuplicate {
  QString code;
  QString existingTitle;
  QString importedTitle;
  QString existingDescription;
  QString importedDescription;
};

class ImportDuplicateDialog : public QDialog {
  Q_OBJECT
public:
  explicit ImportDuplicateDialog(const QList<ImportDuplicate> &duplicates,
                                 const QString &defaultAction,
                                 QWidget *parent = nullptr);
  QMap<QString, QString> decisions() const;
private:
  QTableWidget *m_table;
};

#endif
