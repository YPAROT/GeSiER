#include "docxexportservice.h"
#include "requirementrelationservice.h"
#include "requirementservice.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>
#include <functional>

namespace {
const QString ContentTag="{{GESIER_CONTENT}}";
const QString ReqBegin="{{GESIER_REQUIREMENT_TEMPLATE_BEGIN}}";
const QString ReqEnd="{{GESIER_REQUIREMENT_TEMPLATE_END}}";
QString esc(QString s){return s.replace('&',"&amp;").replace('<',"&lt;").replace('>',"&gt;").replace('"',"&quot;").replace('\'',"&apos;");}
QString unesc(const QString&s){return QTextDocumentFragment::fromHtml(s).toPlainText();}
QString plain(const QString&s){QTextDocument d;d.setHtml(s);return d.toPlainText();}
QString multi(const QString&s){QStringList l=s.split('\n');QString r;for(int i=0;i<l.size();++i){if(i)r+="</w:t><w:br/><w:t xml:space=\"preserve\">";r+=esc(l[i]);}return r;}
struct TN{int at,len;QString text;};
QList<TN> textNodes(const QString&xml){QList<TN> r;QRegularExpression e("<w:t(?:\\s[^>]*)?>([\\s\\S]*?)</w:t>",QRegularExpression::CaseInsensitiveOption);auto it=e.globalMatch(xml);while(it.hasNext()){auto m=it.next();r<<TN{int(m.capturedStart(1)),int(m.capturedLength(1)),unesc(m.captured(1))};}return r;}
QString visible(const QString&xml){QString r;for(const auto&n:textNodes(xml))r+=n.text;return r;}
int countTag(const QString&xml,const QString&tag){int n=0,p=0;const QString v=visible(xml);while((p=v.indexOf(tag,p,Qt::CaseInsensitive))>=0){++n;p+=tag.size();}return n;}
bool replaceTag(QString*xml,const QString&tag,const QString&value,bool all=true,Qt::CaseSensitivity cs=Qt::CaseInsensitive){bool done=false;while(true){const auto ns=textNodes(*xml);QString joined;for(const auto&n:ns)joined+=n.text;const int a=joined.indexOf(tag,0,cs);if(a<0)break;const int b=a+tag.size();int offset=0,first=-1,last=-1,fo=0,lo=0;for(int i=0;i<ns.size();++i){const int end=offset+ns[i].text.size();if(first<0&&a<end){first=i;fo=a-offset;}if(b<=end){last=i;lo=b-offset;break;}offset=end;}if(first<0||last<0)break;QStringList c;for(const auto&n:ns)c<<n.text;if(first==last)c[first]=c[first].left(fo)+value+c[first].mid(lo);else{c[first]=c[first].left(fo)+value;for(int i=first+1;i<last;++i)c[i].clear();c[last]=c[last].mid(lo);}for(int i=ns.size()-1;i>=0;--i)xml->replace(ns[i].at,ns[i].len,i==first?multi(c[i]):esc(c[i]));done=true;if(!all)break;}return done;}
struct Range{int start=-1,end=-1;QString xml;};
QList<Range> paras(const QString&xml){QList<Range>r;QRegularExpression e("<w:p(?:\\s[^>]*)?>[\\s\\S]*?</w:p>",QRegularExpression::CaseInsensitiveOption);auto it=e.globalMatch(xml);while(it.hasNext()){auto m=it.next();r<<Range{int(m.capturedStart()),int(m.capturedEnd()),m.captured()};}return r;}
Range paraWith(const QString&xml,const QString&tag,int from=0){for(const auto&p:paras(xml))if(p.start>=from&&visible(p.xml).contains(tag,Qt::CaseInsensitive))return p;return {};}
QString paragraph(const QString&t,const QString&style={}){const QString p=style.isEmpty()?QString():"<w:pPr><w:pStyle w:val=\""+style+"\"/></w:pPr>";return "<w:p>"+p+"<w:r><w:t xml:space=\"preserve\">"+multi(t)+"</w:t></w:r></w:p>";}
int outline(const QString&p,const QString&styles){QRegularExpression ol("<w:outlineLvl[^>]*w:val=\"(\\d+)\"");auto m=ol.match(p);if(m.hasMatch())return m.captured(1).toInt()+1;QRegularExpression ps("<w:pStyle[^>]*w:val=\"([^\"]+)\"");m=ps.match(p);if(!m.hasMatch())return 0;QRegularExpression sb("<w:style[^>]*w:styleId=\""+QRegularExpression::escape(m.captured(1))+"\"[\\s\\S]*?</w:style>");auto s=sb.match(styles);if(!s.hasMatch())return 0;m=ol.match(s.captured());return m.hasMatch()?m.captured(1).toInt()+1:0;}
int precedingLevel(const QString&xml,int pos,const QString&styles){int n=0;for(const auto&p:paras(xml)){if(p.start>=pos)break;const int v=outline(p.xml,styles);if(v)n=v;}return n;}
QByteArray readAll(const QString&p){QFile f(p);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
QByteArray hash(const QByteArray&b){return QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex();}
QString label(QSqlDatabase db,const QString&t,const QString&c,int id){QSqlQuery q(db);q.prepare("SELECT "+c+" FROM "+t+" WHERE ID=?");q.addBindValue(id);return q.exec()&&q.next()?q.value(0).toString():QString();}
QString verifications(const RequirementRecord&r,QSqlDatabase db){QStringList out;for(const auto&v:r.verifications){QString line=label(db,"REQ_METHOD","METHOD",v.methodId),details;QStringList d;if(!v.level.isEmpty())d<<v.level;if(!v.procedure.isEmpty())d<<v.procedure;if(!v.means.isEmpty())d<<v.means;if(!v.verdict.isEmpty())d<<v.verdict;if(!v.redmine.isEmpty())d<<v.redmine;if(!v.comment.isEmpty())d<<v.comment;if(!d.isEmpty())line+=" — "+d.join(" | ");out<<line;}return out.join('\n');}
QString relations(int id,const QString&connection){QStringList out;RequirementRelationService s(connection);for(const auto&r:s.relations(id)){QString p;if(r.typeCode=="DECOMPOSE"&&!r.outgoing)p="Enfant de ";else if(r.typeCode=="DEPENDS_ON"&&r.outgoing)p="Dépend de ";else if(r.typeCode=="DERIVES_FROM"&&r.outgoing)p="Dérive de ";else continue;QString line=p+r.otherCode;if(!r.otherTitle.isEmpty())line+=" — "+r.otherTitle;if(!r.comment.isEmpty())line+=" ("+r.comment+")";out<<line;}return out.join('\n');}
QString reqBlock(QString model,int id,const QString&connection){QSqlDatabase db=QSqlDatabase::database(connection,false);RequirementService s(connection);const auto r=s.get(id);const QMap<QString,QString> vals={{"{{REQ_ID}}",QString::number(r.id)},{"{{REQ_CODE}}",r.code},{"{{REQ_TITLE}}",r.title},{"{{REQ_DESCRIPTION}}",plain(r.description)},{"{{REQ_TYPE}}",label(db,"REQ_TYPE","TYPE",r.typeId)},{"{{REQ_STATUS}}",label(db,"REQ_STATUS","STATUS",r.statusId)},{"{{REQ_SOURCE}}",r.source},{"{{REQ_PRODUCT_TREES}}",QString(r.productTrees).replace(" | ","\n")},{"{{REQ_APPLICABILITY}}",QString(r.applicability).replace(" | ","\n")},{"{{REQ_VERIFICATIONS}}",verifications(r,db)},{"{{REQ_RELATIONS}}",relations(id,connection)}};for(auto it=vals.cbegin();it!=vals.cend();++it)replaceTag(&model,it.key(),it.value());return model;}
QJsonObject snapshot(const DocumentRecord&d,const QList<DocumentNodeRecord>&nodes){QJsonObject o{{"id",d.id},{"reference",d.reference},{"title",d.title},{"description",d.description}};QJsonArray refs;for(const auto&r:d.secondaryReferences)refs<<r;o["secondaryReferences"]=refs;QJsonObject meta;for(auto it=d.metadata.cbegin();it!=d.metadata.cend();++it)meta[it.key()]=it.value();o["metadata"]=meta;QJsonArray ns;for(const auto&n:nodes){QJsonObject j{{"id",n.id},{"parentId",n.parentId},{"position",n.position},{"type",n.type},{"title",n.title},{"requirementId",n.requirementId},{"requirementCode",n.requirementCode},{"requirementTitle",n.requirementTitle},{"requirementDescription",n.requirementDescription},{"text",n.textContent},{"imageLegend",n.imageLegend}};if(!n.imageData.isEmpty())j["imageBase64"]=QString::fromLatin1(n.imageData.toBase64());ns<<j;}o["nodes"]=ns;return o;}
QString drawing(int id,const QString&rid){return QString(R"(<w:p><w:r><w:drawing><wp:inline xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing" distT="0" distB="0" distL="0" distR="0"><wp:extent cx="5486400" cy="3086100"/><wp:docPr id="%1" name="Image %1"/><a:graphic xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture"><pic:pic xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture"><pic:nvPicPr><pic:cNvPr id="%1" name="Image %1"/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" r:embed="%2"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="5486400" cy="3086100"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>)").arg(id).arg(rid);}
QString watermark(){return R"(<w:p xmlns:v="urn:schemas-microsoft-com:vml" xmlns:o="urn:schemas-microsoft-com:office:office"><w:pPr><w:jc w:val="center"/></w:pPr><w:r><w:pict><v:shape id="GeSiERDraft" o:spid="_x0000_s2049" type="#_x0000_t136" style="position:absolute;margin-left:0;margin-top:0;width:430pt;height:120pt;rotation:315;z-index:-251654144;mso-position-horizontal:center;mso-position-horizontal-relative:margin;mso-position-vertical:center;mso-position-vertical-relative:margin" fillcolor="#d0d0d0" stroked="f"><v:textpath style="font-family:Arial;font-size:1pt" string="DRAFT"/></v:shape></w:pict></w:r></w:p>)";}
QString stylesDefault(){return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:style w:type="paragraph" w:default="1" w:styleId="Normal"><w:name w:val="Normal"/><w:rPr><w:sz w:val="22"/></w:rPr></w:style><w:style w:type="paragraph" w:styleId="Title"><w:name w:val="Title"/><w:rPr><w:b/><w:sz w:val="36"/></w:rPr></w:style><w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/><w:pPr><w:outlineLvl w:val="0"/></w:pPr><w:rPr><w:b/><w:sz w:val="30"/></w:rPr></w:style><w:style w:type="paragraph" w:styleId="Heading2"><w:name w:val="heading 2"/><w:pPr><w:outlineLvl w:val="1"/></w:pPr><w:rPr><w:b/><w:sz w:val="26"/></w:rPr></w:style><w:style w:type="paragraph" w:styleId="Caption"><w:name w:val="Caption"/><w:rPr><w:i/><w:sz w:val="20"/></w:rPr></w:style></w:styles>)";}
QStringList parts(const QMap<QString,QByteArray>&files){QStringList r{"word/document.xml"};for(auto it=files.cbegin();it!=files.cend();++it)if((it.key().startsWith("word/header")||it.key().startsWith("word/footer"))&&it.key().endsWith(".xml"))r<<it.key();return r;}
void replaceParts(QMap<QString,QByteArray>*files,const QString&tag,const QString&value){for(const auto&p:parts(*files)){QString xml=QString::fromUtf8(files->value(p));if(replaceTag(&xml,tag,value))(*files)[p]=xml.toUtf8();}}
struct MetadataTag{QString tag;QString key;};
QList<MetadataTag> metadataTags(const QMap<QString,QByteArray>&files){QList<MetadataTag> out;QSet<QString> seen;QRegularExpression re("\\{\\{GESIER_METADATA:([^}]*)\\}\\}",QRegularExpression::CaseInsensitiveOption);for(const auto&p:parts(files)){auto matches=re.globalMatch(visible(QString::fromUtf8(files[p])));while(matches.hasNext()){const auto m=matches.next();const QString tag=m.captured(0);if(!seen.contains(tag)){seen.insert(tag);out<<MetadataTag{tag,m.captured(1)};}}}return out;}
enum class MetadataMatch{Found,Missing,Ambiguous};
MetadataMatch metadataValue(const QMap<QString,QString>&metadata,const QString&requested,QString*value){const QString key=requested.trimmed();auto exact=metadata.constFind(key);if(exact!=metadata.cend()){*value=exact.value();return MetadataMatch::Found;}QStringList matches;for(auto it=metadata.cbegin();it!=metadata.cend();++it)if(it.key().trimmed().compare(key,Qt::CaseInsensitive)==0)matches<<it.value();if(matches.isEmpty())return MetadataMatch::Missing;if(matches.size()>1)return MetadataMatch::Ambiguous;*value=matches.first();return MetadataMatch::Found;}
void replaceMetadataTags(QMap<QString,QByteArray>*files,const QMap<QString,QString>&metadata){for(const auto&t:metadataTags(*files)){QString value;if(!t.key.trimmed().isEmpty())metadataValue(metadata,t.key,&value);for(const auto&p:parts(*files)){QString xml=QString::fromUtf8(files->value(p));if(replaceTag(&xml,t.tag,value,true,Qt::CaseSensitive))(*files)[p]=xml.toUtf8();}}}
bool hasTocField(const QString &xml) {
  return QRegularExpression("<w:instrText[^>]*>[\\s\\S]*?\\bTOC\\b",
                            QRegularExpression::CaseInsensitiveOption)
      .match(xml).hasMatch();
}
QString tocField() {
  return R"(<w:p><w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText xml:space="preserve"> TOC \o "1-6" \h \z \u </w:instrText></w:r><w:r><w:fldChar w:fldCharType="separate"/></w:r><w:r><w:t>Mettre à jour le sommaire</w:t></w:r><w:r><w:fldChar w:fldCharType="end"/></w:r></w:p>)";
}
void processToc(QString *documentXml) {
  const Range marker = paraWith(*documentXml, "{{GESIER_TOC}}");
  if (marker.start < 0)
    return;
  if (hasTocField(*documentXml)) {
    replaceTag(documentXml, "{{GESIER_TOC}}", QString());
  } else {
    documentXml->replace(marker.start, marker.end - marker.start, tocField());
  }
}
void enableFieldUpdates(QMap<QString,QByteArray> *files, QString *relationships,
                        QString *contentTypes) {
  QString settings = QString::fromUtf8(files->value("word/settings.xml"));
  if (settings.isEmpty()) {
    settings = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><w:settings "
               "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
               "<w:updateFields w:val=\"true\"/></w:settings>";
    if (!relationships->contains("relationships/settings"))
      relationships->replace(
          "</Relationships>",
          "<Relationship Id=\"rIdGesierSettings\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings\" Target=\"settings.xml\"/></Relationships>");
    if (!contentTypes->contains("/word/settings.xml"))
      contentTypes->replace(
          "</Types>",
          "<Override PartName=\"/word/settings.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml\"/></Types>");
  } else if (settings.contains("<w:updateFields")) {
    settings.replace(QRegularExpression("<w:updateFields[^>]*/>"),
                     "<w:updateFields w:val=\"true\"/>");
  } else {
    settings.replace("</w:settings>",
                     "<w:updateFields w:val=\"true\"/></w:settings>");
  }
  (*files)["word/settings.xml"] = settings.toUtf8();
}
void updateTitleFields(QString *xml, const QString &title) {
  const QString escaped = esc(title);
  QRegularExpression complex(
      "(<w:instrText[^>]*>[^<]*DOCPROPERTY\\s+(?:&quot;|\")?Title(?:&quot;|\")?[^<]*</w:instrText>"
      "[\\s\\S]*?<w:fldChar[^>]*w:fldCharType=\"separate\"[^>]*/>[\\s\\S]*?<w:t(?:\\s[^>]*)?>)"
      "[\\s\\S]*?(</w:t>)",
      QRegularExpression::CaseInsensitiveOption);
  xml->replace(complex, "\\1" + escaped + "\\2");
  QRegularExpression simple(
      "(<w:fldSimple[^>]*w:instr=\"[^\"]*DOCPROPERTY\\s+(?:&quot;)?Title(?:&quot;)?[^\"]*\"[^>]*>"
      "[\\s\\S]*?<w:t(?:\\s[^>]*)?>)[\\s\\S]*?(</w:t>)",
      QRegularExpression::CaseInsensitiveOption);
  xml->replace(simple, "\\1" + escaped + "\\2");
}
void updateDocumentTitle(QMap<QString,QByteArray> *files, const QString &title,
                         QString *contentTypes) {
  replaceParts(files, "{{GESIER_DOCUMENT_TITLE}}", title);
  for (const QString &part : parts(*files)) {
    QString xml = QString::fromUtf8(files->value(part));
    updateTitleFields(&xml, title);
    (*files)[part] = xml.toUtf8();
  }
  QString core = QString::fromUtf8(files->value("docProps/core.xml"));
  if (core.isEmpty()) {
    core = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
           "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
           "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"><dc:title>" +
           esc(title) + "</dc:title></cp:coreProperties>";
  } else if (core.contains(QRegularExpression("<dc:title(?:\\s[^>]*)?>",
                                               QRegularExpression::CaseInsensitiveOption))) {
    core.replace(QRegularExpression("<dc:title(?:\\s[^>]*)?>[\\s\\S]*?</dc:title>",
                                    QRegularExpression::CaseInsensitiveOption),
                 "<dc:title>" + esc(title) + "</dc:title>");
  } else {
    core.replace("</cp:coreProperties>",
                 "<dc:title>" + esc(title) + "</dc:title></cp:coreProperties>");
  }
  (*files)["docProps/core.xml"] = core.toUtf8();
  QString rootRelationships = QString::fromUtf8(files->value("_rels/.rels"));
  if (!rootRelationships.contains("metadata/core-properties")) {
    rootRelationships.replace(
        "</Relationships>",
        "<Relationship Id=\"rIdGesierCoreProperties\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/></Relationships>");
    (*files)["_rels/.rels"] = rootRelationships.toUtf8();
  }
  if (!contentTypes->contains("/docProps/core.xml"))
    contentTypes->replace(
        "</Types>",
        "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/></Types>");
}
}

DocxExportService::DocxExportService(QString c):m_connectionName(std::move(c)){}

DocxTemplateValidation DocxExportService::validateTemplate(const QString&path,int documentId)const{
  DocxTemplateValidation v;
  if(path.isEmpty()){v.valid=true;return v;}
  if(!QFileInfo::exists(path)){v.errors<<"Le template DOCX est introuvable.";return v;}
  QZipReader z(path);
  if(!z.exists()){v.errors<<"Le fichier n'est pas un DOCX valide.";return v;}
  QMap<QString,QByteArray> f;
  for(const auto&i:z.fileInfoList())if(i.isFile)f[i.filePath]=z.fileData(i.filePath);
  const QString doc=QString::fromUtf8(f["word/document.xml"]);
  if(doc.isEmpty()){v.errors<<"Le template ne contient pas word/document.xml.";return v;}
  int cc=0;
  for(const auto&p:parts(f))cc+=countTag(QString::fromUtf8(f[p]),ContentTag);
  if(cc!=1)v.errors<<QString("La balise %1 doit apparaître exactement une fois (%2 trouvée(s)).").arg(ContentTag).arg(cc);
  const int bc=countTag(doc,ReqBegin),ec=countTag(doc,ReqEnd);
  if(bc!=1||ec!=1)v.errors<<"Le modèle d'exigence doit contenir exactement une balise de début et une balise de fin.";
  else{const auto b=paraWith(doc,ReqBegin),e=paraWith(doc,ReqEnd,b.end);if(b.start<0||e.start<=b.start)v.errors<<"Les bornes du modèle d'exigence sont absentes ou inversées.";else if(doc.mid(b.end,e.start-b.end).trimmed().isEmpty())v.errors<<"Le modèle d'exigence est vide.";}
  int pc=0;
  for(int n=1;n<=6;++n){const QString tag="{{GESIER_CHAPTER_LEVEL_"+QString::number(n)+"}}";const int c=countTag(doc,tag);pc+=c;if(c>1)v.errors<<tag+" apparaît plusieurs fois.";}
  if(!pc)v.errors<<"Aucun prototype de chapitre GESIER_CHAPTER_LEVEL_1 à _6 n'est défini.";
  for(const QString tag:{"{{GESIER_REFERENCE}}","{{GESIER_METADATA}}"}){int c=0;for(const auto&p:parts(f))c+=countTag(QString::fromUtf8(f[p]),tag);if(!c)v.warnings<<"Zone "+tag+" absente.";}
  const QMap<QString,QString> metadata=documentId>=0?DocumentService(m_connectionName).document(documentId).metadata:QMap<QString,QString>();
  for(const auto&t:metadataTags(f)){const QString key=t.key.trimmed();if(key.isEmpty()){v.warnings<<"La balise "+t.tag+" ne contient aucun nom de métadonnée.";continue;}if(documentId<0)continue;QString value;const auto match=metadataValue(metadata,key,&value);if(match==MetadataMatch::Missing)v.warnings<<"La métadonnée « "+key+" » demandée par le template est absente du document.";else if(match==MetadataMatch::Ambiguous)v.warnings<<"La métadonnée « "+key+" » est ambiguë dans le document.";}
  if(!hasTocField(doc)&&!visible(doc).contains("{{GESIER_TOC}}",Qt::CaseInsensitive))
    v.warnings<<"Aucune table des matières Word ni balise {{GESIER_TOC}} n'est présente.";
  v.valid=v.errors.isEmpty();
  return v;
}

RequirementResult DocxExportService::exportDocument(const DocxExportRequest&r){
  if(r.documentId<0||r.outputPath.trimmed().isEmpty())
    return RequirementResult::failure("Document et fichier de sortie obligatoires.");
  DocumentService ds(m_connectionName);
  const auto d=ds.document(r.documentId);
  if(d.id<0)
    return RequirementResult::failure("Document introuvable.");
  if(r.publication&&(d.reference.trimmed().isEmpty()||r.version.trimmed().isEmpty()||r.title.trimmed().isEmpty()))
    return RequirementResult::failure("Une publication exige une référence, une version et un titre.");
  const QString tpl=r.templatePath.isEmpty()?d.templatePath:r.templatePath;
  const auto validation=validateTemplate(tpl,d.id);
  if(!validation.valid)
    return RequirementResult::failure(validation.errors.join('\n'));
  const auto nodes=ds.nodes(d.id);
  QMap<int,QList<DocumentNodeRecord>> children;
  for(const auto&n:nodes)
    children[n.parentId]<<n;
  QMap<QString,QByteArray> files;QString doc,rels,types,reqModel;QMap<int,QString> prototypes;int base=0;if(!tpl.isEmpty()){QZipReader z(tpl);for(const auto&i:z.fileInfoList())if(i.isFile)files[i.filePath]=z.fileData(i.filePath);doc=QString::fromUtf8(files["word/document.xml"]);rels=QString::fromUtf8(files["word/_rels/document.xml.rels"]);types=QString::fromUtf8(files["[Content_Types].xml"]);const QString styles=QString::fromUtf8(files["word/styles.xml"]);const auto cp=paraWith(doc,ContentTag);base=precedingLevel(doc,cp.start,styles);const auto rb=paraWith(doc,ReqBegin),re=paraWith(doc,ReqEnd,rb.end);reqModel=doc.mid(rb.end,re.start-rb.end);doc.remove(rb.start,re.end-rb.start);for(int n=1;n<=6;++n){const QString tag="{{GESIER_CHAPTER_LEVEL_"+QString::number(n)+"}}";const auto p=paraWith(doc,tag);if(p.start>=0){prototypes[n]=p.xml;doc.remove(p.start,p.end-p.start);}}}else{doc="<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><w:body>"+paragraph(d.title,"Title")+paragraph(ContentTag)+"<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\"/></w:sectPr></w:body></w:document>";files["word/styles.xml"]=stylesDefault().toUtf8();rels="<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rIdStyles\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/></Relationships>";types="<?xml version=\"1.0\" encoding=\"UTF-8\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\"><Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/><Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/><Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/></Types>";files["_rels/.rels"]="<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>";prototypes[1]=paragraph("{{GESIER_CHAPTER_LEVEL_1}}","Heading1");prototypes[2]=paragraph("{{GESIER_CHAPTER_LEVEL_2}}","Heading2");}
  int maxLevel=base;std::function<void(int,int)> depth=[&](int parent,int n){for(const auto&node:children[parent]){if(node.type=="CHAPTER")maxLevel=qMax(maxLevel,base+n);depth(node.id,n+(node.type=="CHAPTER"));}};depth(-1,1);if(!tpl.isEmpty())for(int n=qMax(1,base+1);n<=qMin(6,maxLevel);++n)if(!prototypes.contains(n))return RequirementResult::failure(QString("Le prototype {{GESIER_CHAPTER_LEVEL_%1}} requis par la structure est absent.").arg(n));
  QList<QPair<QString,QByteArray>> images;int imageId=0;std::function<QString(int,int)> render=[&](int parent,int dep){QString out;for(const auto&n:children[parent]){int child=dep;if(n.type=="CHAPTER"){int wanted=qBound(1,base+dep,6),selected=wanted;while(selected>1&&!prototypes.contains(selected))--selected;if(!prototypes.contains(selected))selected=prototypes.lastKey();QString p=prototypes[selected];replaceTag(&p,"{{GESIER_CHAPTER_LEVEL_"+QString::number(selected)+"}}",n.title);out+=p;child=dep+1;}else if(n.type=="REQUIREMENT")out+=reqModel.isEmpty()?paragraph(n.requirementCode+" — "+n.requirementTitle,"Heading2")+paragraph(plain(n.requirementDescription)):reqBlock(reqModel,n.requirementId,m_connectionName);else if(n.type=="TEXT")for(const auto&line:plain(n.textContent).split('\n'))if(!line.trimmed().isEmpty())out+=paragraph(line);else{}else if(n.type=="IMAGE"){++imageId;QBuffer b;b.setData(n.imageData);b.open(QIODevice::ReadOnly);QImageReader ir(&b);QString ext=QString::fromLatin1(ir.format()).toLower();if(ext=="jpeg")ext="jpg";if(ext.isEmpty())ext="png";images<<qMakePair(ext,n.imageData);out+=drawing(imageId,"rIdGesier"+QString::number(imageId));if(!n.imageLegend.isEmpty())out+=paragraph(n.imageLegend,"Caption");}out+=render(n.id,child);}return out;};const QString content=render(-1,1);const auto cp=paraWith(doc,ContentTag);if(cp.start<0)return RequirementResult::failure("La balise GESIER_CONTENT est introuvable dans le corps du document.");doc.replace(cp.start,cp.end-cp.start,content);files["word/document.xml"]=doc.toUtf8();QString refs=d.reference;if(!d.secondaryReferences.isEmpty())refs+="\n"+d.secondaryReferences.join('\n');QStringList meta;for(auto it=d.metadata.cbegin();it!=d.metadata.cend();++it)meta<<it.key()+" : "+it.value();replaceParts(&files,"{{GESIER_REFERENCE}}",refs);replaceParts(&files,"{{GESIER_METADATA}}",meta.join('\n'));doc=QString::fromUtf8(files["word/document.xml"]);processToc(&doc);files["word/document.xml"]=doc.toUtf8();const QString exportedTitle=r.publication?r.title:d.title;updateDocumentTitle(&files,exportedTitle,&types);doc=QString::fromUtf8(files["word/document.xml"]);
  replaceMetadataTags(&files,d.metadata);
  doc=QString::fromUtf8(files["word/document.xml"]);
  if(rels.isEmpty())
    rels="<?xml version=\"1.0\" encoding=\"UTF-8\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"></Relationships>";
  for(int i=0;i<images.size();++i){const QString ext=images[i].first;files["word/media/gesier"+QString::number(i+1)+"."+ext]=images[i].second;rels.replace("</Relationships>",QString("<Relationship Id=\"rIdGesier%1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/gesier%1.%2\"/></Relationships>").arg(i+1).arg(ext));if(!types.contains("Extension=\""+ext+"\""))types.replace("</Types>","<Default Extension=\""+ext+"\" ContentType=\"image/"+(ext=="jpg"?"jpeg":ext)+"\"/></Types>");}
  enableFieldUpdates(&files,&rels,&types);
  if(!r.publication){QString hn;for(auto it=files.cbegin();it!=files.cend();++it)if(it.key().startsWith("word/header")&&it.key().endsWith(".xml")){hn=it.key();break;}if(!hn.isEmpty()){QString h=QString::fromUtf8(files[hn]);h.replace("</w:hdr>",watermark()+"</w:hdr>");files[hn]=h.toUtf8();}else{files["word/headerGesier.xml"]=("<?xml version=\"1.0\" encoding=\"UTF-8\"?><w:hdr xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"+watermark()+"</w:hdr>").toUtf8();rels.replace("</Relationships>","<Relationship Id=\"rIdGesierHeader\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/header\" Target=\"headerGesier.xml\"/></Relationships>");types.replace("</Types>","<Override PartName=\"/word/headerGesier.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.header+xml\"/></Types>");doc.replace("<w:sectPr>","<w:sectPr><w:headerReference w:type=\"default\" r:id=\"rIdGesierHeader\"/>");}}
  files["word/document.xml"]=doc.toUtf8();files["word/_rels/document.xml.rels"]=rels.toUtf8();files["[Content_Types].xml"]=types.toUtf8();QZipWriter zw(r.outputPath);for(auto it=files.cbegin();it!=files.cend();++it)zw.addFile(it.key(),it.value());zw.close();if(zw.status()!=QZipWriter::NoError)return RequirementResult::failure("Impossible d'écrire le DOCX.");
  const QByteArray bytes=readAll(r.outputPath),fh=hash(bytes),tb=readAll(tpl),snap=QJsonDocument(snapshot(d,nodes)).toJson(QJsonDocument::Compact);QJsonObject mo;for(auto it=d.metadata.cbegin();it!=d.metadata.cend();++it)mo[it.key()]=it.value();QSqlQuery q(QSqlDatabase::database(m_connectionName,false));q.prepare("INSERT INTO DOCUMENT_EXPORT(DOC_ID,EXPORT_KIND,VERSION,TITLE,AUTHOR,FILE_PATH,FILE_SHA256,SNAPSHOT_JSON,TEMPLATE_PATH,TEMPLATE_SHA256,METADATA_JSON,DRAFT_LABEL) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");q.addBindValue(d.id);q.addBindValue(r.publication?"PUBLICATION":"DRAFT");q.addBindValue(r.publication?r.version:QVariant());q.addBindValue(r.publication?r.title:(r.draftLabel.isEmpty()?d.title:r.draftLabel));q.addBindValue(r.author);q.addBindValue(QFileInfo(r.outputPath).absoluteFilePath());q.addBindValue(QString::fromLatin1(fh));q.addBindValue(r.publication?QString::fromUtf8(snap):QVariant());q.addBindValue(tpl);q.addBindValue(tpl.isEmpty()?QString():QString::fromLatin1(hash(tb)));q.addBindValue(QString::fromUtf8(QJsonDocument(mo).toJson(QJsonDocument::Compact)));q.addBindValue(r.draftLabel);if(!q.exec())return RequirementResult::failure(q.lastError().text());return RequirementResult::successResult(r.publication?"Publication créée.":"Draft exporté.",q.lastInsertId().toInt());
}

QList<DocumentExportRecord> DocxExportService::history(int id)const{QList<DocumentExportRecord> out;QSqlQuery q(QSqlDatabase::database(m_connectionName,false));q.prepare("SELECT ID,DOC_ID,EXPORT_KIND,COALESCE(VERSION,''),COALESCE(TITLE,''),COALESCE(AUTHOR,''),EXPORTED_AT,COALESCE(FILE_PATH,''),COALESCE(FILE_SHA256,''),COALESCE(GED_REFERENCE,''),COALESCE(GED_LINK,'') FROM DOCUMENT_EXPORT WHERE DOC_ID=? ORDER BY ID DESC");q.addBindValue(id);if(q.exec())while(q.next()){DocumentExportRecord r;r.id=q.value(0).toInt();r.documentId=q.value(1).toInt();r.kind=q.value(2).toString();r.version=q.value(3).toString();r.title=q.value(4).toString();r.author=q.value(5).toString();r.exportedAt=q.value(6).toString();r.filePath=q.value(7).toString();r.sha256=q.value(8).toString();r.gedReference=q.value(9).toString();r.gedLink=q.value(10).toString();out<<r;}return out;}
RequirementResult DocxExportService::setGedInformation(int id,const QString&ref,const QString&link){QSqlQuery q(QSqlDatabase::database(m_connectionName,false));q.prepare("UPDATE DOCUMENT_EXPORT SET GED_REFERENCE=?,GED_LINK=? WHERE ID=? AND EXPORT_KIND='PUBLICATION'");q.addBindValue(ref.trimmed());q.addBindValue(link.trimmed());q.addBindValue(id);if(!q.exec())return RequirementResult::failure(q.lastError().text());if(q.numRowsAffected()!=1)return RequirementResult::failure("Publication introuvable.");return RequirementResult::successResult("Informations GED enregistrées.",id);}
bool DocxExportService::verifyHash(int id,QString*error)const{QSqlQuery q(QSqlDatabase::database(m_connectionName,false));q.prepare("SELECT FILE_PATH,FILE_SHA256 FROM DOCUMENT_EXPORT WHERE ID=?");q.addBindValue(id);if(!q.exec()||!q.next()){if(error)*error="Export introuvable.";return false;}const QByteArray b=readAll(q.value(0).toString());if(b.isEmpty()){if(error)*error="Fichier exporté introuvable ou vide.";return false;}const bool ok=QString::fromLatin1(hash(b)).compare(q.value(1).toString(),Qt::CaseInsensitive)==0;if(!ok&&error)*error="L'empreinte SHA-256 ne correspond pas.";return ok;}
