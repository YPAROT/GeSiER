#ifndef TABULAREXPORTDIALOG_H
#define TABULAREXPORTDIALOG_H

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QTableWidget;

class TabularExportDialog : public QDialog {
  Q_OBJECT
public:
  TabularExportDialog(const QString &scope, const QStringList &headers,
                      QWidget *parent = nullptr);
  QList<int> columns() const;
  QStringList outputHeaders() const;
  QString sheetName() const;

public slots:
  void accept() override;

private:
  void moveCurrent(int offset);
  QString m_scope;
  QTableWidget *m_columns;
  QLineEdit *m_sheetName;
};

#endif
