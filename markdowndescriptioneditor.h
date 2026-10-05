#ifndef MARKDOWNDESCRIPTIONEDITOR_H
#define MARKDOWNDESCRIPTIONEDITOR_H

#include "requirementservice.h"
#include <QWidget>

class QPlainTextEdit;
class QPushButton;
class QTextBrowser;
class QStackedWidget;

class MarkdownDescriptionEditor : public QWidget {
  Q_OBJECT
public:
  explicit MarkdownDescriptionEditor(QWidget *parent = nullptr);
  QString markdown() const;
  QList<RequirementImage> images() const { return m_images; }
  void setContent(const QString &markdown,
                  const QList<RequirementImage> &images = {});
  void setEditing(bool editing);
  bool isEditing() const;
  static QString plainText(const QString &markdown);

signals:
  void contentChanged();

private:
  void refreshPreview();
  void wrapSelection(const QString &left, const QString &right,
                     const QString &placeholder);
  void insertTable();
  void insertImage();
  QPlainTextEdit *m_source;
  QTextBrowser *m_preview;
  QStackedWidget *m_stack;
  QPushButton *m_toggle;
  QWidget *m_toolbar;
  QList<RequirementImage> m_images;
  bool m_loading = false;
};

#endif
