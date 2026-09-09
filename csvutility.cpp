#include "csvutility.h"

#include <QFile>
#include <QTextStream>

QList<QStringList> CsvUtility::read(const QString &filename, QChar separator, QString *errorMessage)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage=file.errorString();
        return {};
    }
    const QString text=QString::fromUtf8(file.readAll());
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
