#include "markdownutility.h"

#include <QRegularExpression>
#include <QGuiApplication>
#include <QTextDocument>
#include <QUuid>

namespace {
constexpr auto dialect = QTextDocument::MarkdownDialectGitHub;
}

QString MarkdownUtility::fromHtml(const QString &html) {
  if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
    QString markdown = html;
    markdown.replace(QRegularExpression("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
    markdown.replace(QRegularExpression("</p\\s*>", QRegularExpression::CaseInsensitiveOption), "\n\n");
    markdown.replace(QRegularExpression("<(?:b|strong)(?:\\s[^>]*)?>", QRegularExpression::CaseInsensitiveOption), "**");
    markdown.replace(QRegularExpression("</(?:b|strong)\\s*>", QRegularExpression::CaseInsensitiveOption), "**");
    markdown.replace(QRegularExpression("<(?:i|em)(?:\\s[^>]*)?>", QRegularExpression::CaseInsensitiveOption), "*");
    markdown.replace(QRegularExpression("</(?:i|em)\\s*>", QRegularExpression::CaseInsensitiveOption), "*");
    markdown.replace(QRegularExpression("<li(?:\\s[^>]*)?>", QRegularExpression::CaseInsensitiveOption), "- ");
    markdown.replace(QRegularExpression("</li\\s*>", QRegularExpression::CaseInsensitiveOption), "\n");
    markdown.remove(QRegularExpression("<[^>]+>"));
    return markdown.trimmed();
  }
  QTextDocument document;
  document.setHtml(html);
  return document.toMarkdown(dialect).trimmed();
}

QString MarkdownUtility::toPlainText(const QString &markdown) {
  if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
    QString text = markdown;
    text.replace(QRegularExpression(R"(!\[([^\]]*)\]\([^)]*\))"), "\\1");
    text.replace(QRegularExpression(R"(\[([^\]]+)\]\([^)]*\))"), "\\1");
    text.remove(QRegularExpression(R"((^|\n)\s{0,3}#{1,6}\s*)"));
    text.remove(QRegularExpression(R"((\*\*|__|\*|_|~~|`))"));
    text.replace('|', ' ');
    return text.simplified();
  }
  QTextDocument document;
  document.setMarkdown(markdown, dialect);
  return document.toPlainText();
}

QString MarkdownUtility::normalizeLegacy(const QString &value) {
  static const QRegularExpression html(
      "<(?:p|br|div|span|b|strong|i|em|ul|ol|li|table|tr|td|th)(?:\\s|>|/)",
      QRegularExpression::CaseInsensitiveOption);
  return html.match(value).hasMatch() ? fromHtml(value) : value;
}

QString MarkdownUtility::imageUrl(const QString &token) {
  return "gesier-image://" + token;
}

QString MarkdownUtility::tokenFromUrl(const QString &url) {
  const QString prefix = "gesier-image://";
  return url.startsWith(prefix, Qt::CaseInsensitive) ? url.mid(prefix.size())
                                                      : QString();
}

QStringList MarkdownUtility::imageTokens(const QString &markdown) {
  QStringList result;
  QRegularExpression expression(
      R"(!\[[^\]]*\]\(\s*<?gesier-image://([^\s)>]+)>?(?:\s+["'][^"']*["'])?\s*\))",
      QRegularExpression::CaseInsensitiveOption);
  auto matches = expression.globalMatch(markdown);
  while (matches.hasNext()) {
    const QString token = matches.next().captured(1);
    if (!result.contains(token)) result << token;
  }
  return result;
}

QString MarkdownUtility::duplicatedMarkdown(
    const QString &markdown, QList<RequirementImage> *images) {
  QString result = markdown;
  if (!images) return result;
  for (RequirementImage &image : *images) {
    const QString oldToken = image.token;
    image.token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    result.replace(imageUrl(oldToken), imageUrl(image.token),
                   Qt::CaseInsensitive);
  }
  return result;
}
