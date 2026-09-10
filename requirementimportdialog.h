#ifndef REQUIREMENTIMPORTDIALOG_H
#define REQUIREMENTIMPORTDIALOG_H

#include "xlsxreader.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

class RequirementImportDialog : public QDialog {
  Q_OBJECT
public:
  explicit RequirementImportDialog(const QString &connectionName,
                                   QWidget *parent = nullptr);

signals:
  void imported();

private slots:
  void chooseFile();
  void refreshPreview();
  void runImport();

private:
  void rebuildMappings();
  int mappedColumn(int field) const;
  int resolveLevel(const QString &value, int occurrences,
                   QMap<QString, int> &decisions, bool *rejected);
  QString m_connection;
  QList<XlsxSheet> m_sheets;
  QLabel *m_fileLabel;
  QComboBox *m_sheet;
  QSpinBox *m_headerRow;
  QTableWidget *m_preview;
  QTableWidget *m_mapping;
  QCheckBox *m_external;
  QCheckBox *m_rememberMapping;
  QComboBox *m_duplicates;
  QComboBox *m_documentMode;
  QComboBox *m_existingDocument;
  QLineEdit *m_documentReference;
  QLineEdit *m_documentTitle;
  QLineEdit *m_documentDescription;
};

#endif
