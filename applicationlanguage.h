#ifndef APPLICATIONLANGUAGE_H
#define APPLICATIONLANGUAGE_H

#include <QSettings>
#include <QString>

namespace ApplicationLanguage {
inline constexpr auto SettingsKey = "application/language";

inline QString &storedLanguage() {
  static QString language = [] {
    const QString saved =
        QSettings().value(SettingsKey, QStringLiteral("fr")).toString().toLower();
    return saved == "en" ? QStringLiteral("en") : QStringLiteral("fr");
  }();
  return language;
}

inline QString current() {
  return storedLanguage();
}

inline void setCurrent(const QString &language) {
  storedLanguage() = language.toLower() == "en" ? QStringLiteral("en")
                                                  : QStringLiteral("fr");
  QSettings settings;
  settings.setValue(SettingsKey, storedLanguage());
  settings.sync();
}

inline QString relationLabel(const QString &code) {
  const bool english = current() == "en";
  if (code == "DECOMPOSE")
    return english ? QStringLiteral("Child of") : QStringLiteral("Enfant de");
  if (code == "DERIVES_FROM")
    return english ? QStringLiteral("Derives from") : QStringLiteral("Dérive de");
  return english ? QStringLiteral("Depends on") : QStringLiteral("Dépend de");
}
}

#endif
