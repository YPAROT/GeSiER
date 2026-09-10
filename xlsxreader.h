#ifndef XLSXREADER_H
#define XLSXREADER_H

#include <QString>
#include <QStringList>
#include <QVector>

struct XlsxSheet {
  QString name;
  QVector<QStringList> rows;
};

class XlsxReader {
public:
  static QList<XlsxSheet> read(const QString &fileName,
                               QString *error = nullptr);
};

#endif
