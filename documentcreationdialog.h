#ifndef DOCUMENTCREATIONDIALOG_H
#define DOCUMENTCREATIONDIALOG_H

#include "documentservice.h"
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;
class QTextEdit;

class DocumentCreationDialog : public QDialog {
  Q_OBJECT
public:
  explicit DocumentCreationDialog(const QString &connectionName,
                                  QWidget *parent = nullptr);
  DocumentRecord record() const;
  DocumentCreationOptions options() const;

private:
  QLineEdit *m_reference;
  QLineEdit *m_secondary;
  QLineEdit *m_title;
  QLineEdit *m_template;
  QTextEdit *m_description;
  QComboBox *m_type;
  QComboBox *m_pt;
  QTableWidget *m_metadata;
  QCheckBox *m_attachPt;
  QCheckBox *m_includeChildren;
};

#endif
