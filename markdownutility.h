#ifndef MARKDOWNUTILITY_H
#define MARKDOWNUTILITY_H

#include "requirementservice.h"
#include <QString>

namespace MarkdownUtility {
QString fromHtml(const QString &html);
QString toPlainText(const QString &markdown);
QString normalizeLegacy(const QString &value);
QStringList imageTokens(const QString &markdown);
QString imageUrl(const QString &token);
QString tokenFromUrl(const QString &url);
QString duplicatedMarkdown(const QString &markdown,
                           QList<RequirementImage> *images);
}

#endif
