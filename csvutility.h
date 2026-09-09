#ifndef CSVUTILITY_H
#define CSVUTILITY_H

#include <QList>
#include <QString>
#include <QStringList>

namespace CsvUtility {
QList<QStringList> read(const QString &filename, QChar separator, QString *errorMessage = nullptr);
bool write(const QString &filename, const QList<QStringList> &rows, QChar separator, QString *errorMessage = nullptr);
}

#endif // CSVUTILITY_H
