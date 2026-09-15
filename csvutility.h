#ifndef CSVUTILITY_H
#define CSVUTILITY_H

#include <QList>
#include <QString>
#include <QStringList>

namespace CsvUtility {
enum class CsvEncoding { Auto, Utf8, Windows1252 };
}
using CsvEncoding = CsvUtility::CsvEncoding;
struct CsvReadOptions {
  QChar separator;
  CsvEncoding encoding = CsvEncoding::Auto;
};
namespace CsvUtility {
QList<QStringList> read(const QString &filename, QChar separator, QString *errorMessage = nullptr);
QList<QStringList> read(const QString &filename, const CsvReadOptions &options,
                        QString *errorMessage = nullptr);
bool write(const QString &filename, const QList<QStringList> &rows, QChar separator, QString *errorMessage = nullptr);
}

#endif // CSVUTILITY_H
