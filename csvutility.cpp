#include "csvutility.h"

#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QTextStream>

namespace {
QChar detectedSeparator(const QString &text) {
    const QString firstLine = text.section(QRegularExpression("[\\r\\n]"), 0, 0);
    QMap<QChar, int> counts;
    for (QChar candidate : {QChar(';'), QChar(','), QChar('\t')}) {
        bool quoted = false;
        for (QChar ch : firstLine) {
            if (ch == '"') quoted = !quoted;
            else if (!quoted && ch == candidate) ++counts[candidate];
        }
    }
    QChar best = ';';
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        if (it.value() > counts.value(best)) best = it.key();
    return best;
}
QString decodeCsv(const QByteArray &bytes, CsvEncoding encoding) {
    auto windows1252 = [](const QByteArray &data) {
        static const ushort controls[] = {
            0x20ac,0x0081,0x201a,0x0192,0x201e,0x2026,0x2020,0x2021,
            0x02c6,0x2030,0x0160,0x2039,0x0152,0x008d,0x017d,0x008f,
            0x0090,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
            0x02dc,0x2122,0x0161,0x203a,0x0153,0x009d,0x017e,0x0178};
        QString result; result.reserve(data.size());
        for (unsigned char byte : data)
            result += byte >= 0x80 && byte <= 0x9f
                          ? QChar(controls[byte - 0x80]) : QChar(byte);
        return result;
    };
    if (bytes.startsWith(QByteArray::fromHex("efbbbf"))) return QString::fromUtf8(bytes.mid(3));
    if (encoding == CsvEncoding::Utf8) return QString::fromUtf8(bytes);
    if (encoding == CsvEncoding::Windows1252) return windows1252(bytes);
    const QString utf8 = QString::fromUtf8(bytes);
    return utf8.contains(QChar::ReplacementCharacter) ? windows1252(bytes) : utf8;
}
}

QList<QStringList> CsvUtility::read(const QString &filename, QChar separator, QString *errorMessage)
{
    CsvReadOptions options; options.separator = separator;
    return read(filename, options, errorMessage);
}

QList<QStringList> CsvUtility::read(const QString &filename, const CsvReadOptions &options, QString *errorMessage)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage=file.errorString();
        return {};
    }
    const QString text=decodeCsv(file.readAll(), options.encoding);
    const QChar separator=options.separator.isNull() ? detectedSeparator(text) : options.separator;
    QList<QStringList> rows; QStringList row; QString field; bool quoted=false;
    for(int i=0;i<text.size();++i){
        const QChar ch=text.at(i);
        if(ch=='"'){
            if(quoted && i+1<text.size() && text.at(i+1)=='"'){field+='"';++i;}
            else quoted=!quoted;
        } else if(ch==separator && !quoted){row<<field;field.clear();}
        else if((ch=='\n'||ch=='\r')&&!quoted){
            if(ch=='\r'&&i+1<text.size()&&text.at(i+1)=='\n')++i;
            row<<field;field.clear(); if(!(row.size()==1&&row.first().isEmpty()))rows<<row;row.clear();
        } else field+=ch;
    }
    if(quoted){if(errorMessage)*errorMessage="Champ CSV entre guillemets non terminé.";return {};}
    if(!field.isEmpty()||!row.isEmpty()){row<<field;rows<<row;}
    return rows;
}

bool CsvUtility::write(const QString &filename, const QList<QStringList> &rows, QChar separator, QString *errorMessage)
{
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)){
        if(errorMessage) *errorMessage=file.errorString();
        return false;
    }
    QTextStream stream(&file); stream.setEncoding(QStringConverter::Utf8); stream<<QChar(0xfeff);
    for(const QStringList &row:rows){
        QStringList encoded;
        for(QString value:row){
            if(value.contains('"'))value.replace("\"","\"\"");
            if(value.contains(separator)||value.contains('"')||value.contains('\n')||value.contains('\r'))value='"'+value+'"';
            encoded<<value;
        }
        stream<<encoded.join(separator)<<"\r\n";
    }
    return stream.status()==QTextStream::Ok;
}
