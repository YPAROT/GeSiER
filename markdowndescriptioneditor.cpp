#include "markdowndescriptioneditor.h"
#include "markdownutility.h"

#include <QBuffer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>

class LocalMarkdownBrowser final : public QTextBrowser {
public:
  using QTextBrowser::QTextBrowser;
protected:
  QVariant loadResource(int type, const QUrl &name) override {
    if (type == QTextDocument::ImageResource &&
        name.scheme().compare("gesier-image", Qt::CaseInsensitive) != 0)
      return {};
    return QTextBrowser::loadResource(type, name);
  }
};

MarkdownDescriptionEditor::MarkdownDescriptionEditor(QWidget *parent)
    : QWidget(parent) {
  setObjectName("markdownDescriptionEditor");
  m_toggle = new QPushButton("Modifier");
  m_toggle->setObjectName("markdownModeToggle");
  auto *bold = new QPushButton("Gras");
  bold->setObjectName("markdownBold");
  auto *italic = new QPushButton("Italique");
  italic->setObjectName("markdownItalic");
  auto *table = new QPushButton("Tableau");
  table->setObjectName("markdownTable");
  auto *image = new QPushButton("Image…");
  image->setObjectName("markdownImage");
  m_toolbar = new QWidget;
  auto *tools = new QHBoxLayout(m_toolbar);
  tools->setContentsMargins(0, 0, 0, 0);
  tools->addWidget(bold); tools->addWidget(italic); tools->addWidget(table);
  tools->addWidget(image); tools->addStretch();
  m_source = new QPlainTextEdit;
  m_source->setObjectName("markdownSource");
  m_preview = new LocalMarkdownBrowser;
  m_preview->setObjectName("markdownPreview");
  m_preview->setOpenExternalLinks(false);
  m_stack = new QStackedWidget;
  m_stack->addWidget(m_preview); m_stack->addWidget(m_source);
  auto *top = new QHBoxLayout;
  top->addWidget(m_toggle); top->addWidget(m_toolbar, 1);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(top); layout->addWidget(m_stack, 1);
  connect(m_toggle, &QPushButton::clicked, this,
          [this] { setEditing(!isEditing()); });
  connect(bold, &QPushButton::clicked, this,
          [this] { wrapSelection("**", "**", "texte en gras"); });
  connect(italic, &QPushButton::clicked, this,
          [this] { wrapSelection("*", "*", "texte en italique"); });
  connect(table, &QPushButton::clicked, this,
          &MarkdownDescriptionEditor::insertTable);
  connect(image, &QPushButton::clicked, this,
          &MarkdownDescriptionEditor::insertImage);
  connect(m_source, &QPlainTextEdit::textChanged, this, [this] {
    if (!m_loading) emit contentChanged();
  });
  setEditing(false);
}

QString MarkdownDescriptionEditor::markdown() const {
  return m_source->toPlainText();
}

QString MarkdownDescriptionEditor::plainText(const QString &markdown) {
  return MarkdownUtility::toPlainText(markdown);
}

void MarkdownDescriptionEditor::setContent(
    const QString &markdown, const QList<RequirementImage> &images) {
  m_loading = true;
  m_images = images;
  m_source->setPlainText(markdown);
  refreshPreview();
  setEditing(false);
  m_loading = false;
}

bool MarkdownDescriptionEditor::isEditing() const {
  return m_stack->currentWidget() == m_source;
}

void MarkdownDescriptionEditor::setEditing(bool editing) {
  if (!editing) refreshPreview();
  m_stack->setCurrentWidget(editing ? static_cast<QWidget *>(m_source)
                                    : static_cast<QWidget *>(m_preview));
  m_toolbar->setVisible(editing);
  m_toggle->setText(editing ? "Voir" : "Modifier");
  if (editing) m_source->setFocus();
}

void MarkdownDescriptionEditor::refreshPreview() {
  m_preview->clear();
  const QString display = markdown();
  m_preview->document()->setMarkdown(display,
                                     QTextDocument::MarkdownDialectGitHub);
  for (const RequirementImage &image : m_images) {
    QImage decoded;
    if (decoded.loadFromData(image.data))
      m_preview->document()->addResource(
          QTextDocument::ImageResource,
          QUrl(MarkdownUtility::imageUrl(image.token)), decoded);
  }
  for (const QString &token : MarkdownUtility::imageTokens(display)) {
    const bool found = std::any_of(m_images.cbegin(), m_images.cend(),
                                   [&](const RequirementImage &image) {
                                     return image.token == token;
                                   });
    if (!found) {
      QImage missing(240, 48, QImage::Format_ARGB32_Premultiplied);
      missing.fill(QColor(255, 245, 220));
      QPainter painter(&missing);
      painter.setPen(QColor(150, 70, 0));
      painter.drawRect(missing.rect().adjusted(0, 0, -1, -1));
      painter.drawText(missing.rect(), Qt::AlignCenter, "Image introuvable");
      m_preview->document()->addResource(
          QTextDocument::ImageResource,
          QUrl(MarkdownUtility::imageUrl(token)), missing);
    }
  }
  m_preview->document()->markContentsDirty(0,
                                           m_preview->document()->characterCount());
}

void MarkdownDescriptionEditor::wrapSelection(const QString &left,
                                               const QString &right,
                                               const QString &placeholder) {
  QTextCursor cursor = m_source->textCursor();
  QString selected = cursor.selectedText();
  if (selected.isEmpty()) selected = placeholder;
  cursor.insertText(left + selected + right);
  m_source->setTextCursor(cursor);
}

void MarkdownDescriptionEditor::insertTable() {
  QTextCursor cursor = m_source->textCursor();
  cursor.insertText("\n| Colonne 1 | Colonne 2 |\n| --- | --- |\n| Valeur 1 | Valeur 2 |\n");
}

void MarkdownDescriptionEditor::insertImage() {
  const QString path = QFileDialog::getOpenFileName(
      this, "Insérer une image", {}, "Images (*.png *.jpg *.jpeg *.bmp)");
  if (path.isEmpty()) return;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Image", file.errorString()); return;
  }
  RequirementImage image;
  image.data = file.readAll();
  QImage decoded;
  if (!decoded.loadFromData(image.data)) {
    QMessageBox::warning(this, "Image", "Le fichier image est invalide.");
    return;
  }
  image.legend = QInputDialog::getText(this, "Légende de l’image",
                                       "Légende / texte alternatif");
  image.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
  image.mimeType = QMimeDatabase().mimeTypeForFile(path).name();
  m_images << image;
  QTextCursor cursor = m_source->textCursor();
  cursor.insertText(QString("![%1](%2)\n")
                        .arg(image.legend, MarkdownUtility::imageUrl(image.token)));
  emit contentChanged();
}
