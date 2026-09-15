#ifndef REQUIREMENTWIDGET_H
#define REQUIREMENTWIDGET_H
#include "requirementservice.h"
#include <QWidget>
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTextEdit;
class QSplitter;
class QTabWidget;
class QListWidget;
class QGraphicsView;
class QSpinBox;
class CheckableComboBox;
class RequirementWidget : public QWidget {
  Q_OBJECT
public:
  explicit RequirementWidget(QWidget *p = nullptr);
  void setConnectionName(const QString &);
public slots:
  void refresh();
  void applyFilter(const RequirementFilter &);
  void openRequirement(int id);
  void openRequirementApplicability(int id);
signals:
  void dataChanged();
  void openDocumentRequested(int id);
  void openChangesRequested(int id);
private slots:
  void selectRow(int, int);
  void save();
  void cancel();
  void create();
  void obsolete();
  void filtersChanged();
  void allocationChanged();
  void primaryChanged(int);
  void addVerification();
  void removeVerification();
  void moveVerificationUp();
  void moveVerificationDown();
  void sortByColumn(int);

private:
  void loadLookups();
  void loadPt();
  void loadEditor(int);
  void clearEditor();
  void setEditorEnabled(bool);
  void rebuildPrimary(int preferred = -1);
  void populateList();
  void loadRelations(int id);
  void loadRelationGraph(int id);
  void loadDocuments(int id);
  void loadApplicability(const QList<int> &selected);
  void appendVerification(
      const RequirementVerification &verification = RequirementVerification());
  RequirementRecord editorRecord() const;
  bool confirmDiscard();
  QComboBox *methodCombo(int selected = -1) const;
  QComboBox *levelCombo(int selected = -1, const QString &legacy = {}) const;
  QComboBox *verdictCombo(const QString &selected = QString()) const;
  QString m_connection;
  RequirementService m_service;
  RequirementFilter m_filter;
  QList<RequirementRecord> m_records;
  int m_current = -1;
  bool m_dirty = false, m_loading = false;
  int m_sortColumn = 0;
  Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
  QWidget *m_editor;
  QLineEdit *m_code, *m_title, *m_source;
  QTextEdit *m_description;
  QLabel *m_codeWarning;
  QComboBox *m_status, *m_type, *m_primary;
  QTreeWidget *m_pt;
  QTableWidget *m_verifications, *m_list;
  QTableWidget *m_relations, *m_documents;
  QGraphicsView *m_relationGraph;
  QSpinBox *m_graphDepth;
  QListWidget *m_configurations;
  QPushButton *m_save, *m_cancel, *m_duplicate, *m_obsoleteButton;
  QSplitter *m_splitter;
  QTabWidget *m_tabs;
  QLineEdit *m_codeFilter, *m_titleFilter, *m_descriptionFilter,
      *m_sourceFilter;
  CheckableComboBox *m_statusFilter, *m_typeFilter, *m_ptFilter,
      *m_methodFilter, *m_applicabilityFilter;
  CheckableComboBox *m_allocatedFilter, *m_tracedFilter, *m_documentedFilter,
      *m_verifiedFilter;
  QCheckBox *m_includeObsolete;
};
#endif
