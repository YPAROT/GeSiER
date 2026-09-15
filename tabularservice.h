#ifndef TABULARSERVICE_H
#define TABULARSERVICE_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

enum class TabularFormat { Auto, Csv, Xlsx };
enum class TabularEncoding { Auto, Utf8, Windows1252 };
enum class TabularTransform { None, Trim, Uppercase, Lowercase };

struct TabularSheet {
  QString name;
  QList<QStringList> rows;
};

struct TabularWorkbook {
  QList<TabularSheet> sheets;
};

struct TabularReadOptions {
  TabularFormat format = TabularFormat::Auto;
  TabularEncoding encoding = TabularEncoding::Auto;
  QChar separator;
};

struct TabularColumnMapping {
  QString target;
  QString sourceHeader;
  QString outputHeader;
  int sourceColumn = -1;
  TabularTransform transform = TabularTransform::Trim;
};

struct TabularProfile {
  QString name;
  TabularFormat format = TabularFormat::Auto;
  QString sheetName;
  int headerRow = 0;
  int ignoredRows = 0;
  QChar separator;
  TabularEncoding encoding = TabularEncoding::Auto;
  QList<TabularColumnMapping> mappings;
};

struct TabularIssue {
  QString category;
  QString value;
  QString message;
  QList<int> lines;
};

struct TabularReport {
  int totalRows = 0;
  int validRows = 0;
  int createdRows = 0;
  int updatedRows = 0;
  int skippedRows = 0;
  int rejectedRows = 0;
  QList<TabularIssue> issues;
};

class TabularService {
public:
  static TabularWorkbook read(const QString &path,
                              const TabularReadOptions &options = {},
                              QString *error = nullptr);
  static bool writeXlsx(const QString &path,
                        const QList<TabularSheet> &sheets,
                        QString *error = nullptr);
  static bool writeCsv(const QString &path, const TabularSheet &sheet,
                       QChar separator = ';', QString *error = nullptr);
  static QString transform(QString value, TabularTransform transformation);
  static QByteArray serializeProfile(const TabularProfile &profile);
  static bool deserializeProfile(const QByteArray &data,
                                 TabularProfile *profile,
                                 QString *error = nullptr);
  static void saveProfile(const QString &scope, const TabularProfile &profile);
  static TabularProfile loadProfile(const QString &scope,
                                    const QString &name = QString());
};

#endif
