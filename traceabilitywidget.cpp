#include "traceabilitywidget.h"

#include <QComboBox>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

TraceabilityWidget::TraceabilityWidget(QWidget *parent) : QWidget(parent)
{
    m_requirement = new QComboBox(this); m_requirement->setEditable(true);
    m_target = new QComboBox(this); m_target->setEditable(true);
    m_relationType = new QComboBox(this);
    m_depth = new QSpinBox(this); m_depth->setRange(1,5); m_depth->setValue(2);
    m_add = new QPushButton("Ajouter le lien", this);
    m_remove = new QPushButton("Supprimer le lien sélectionné", this);

    QHBoxLayout *selection = new QHBoxLayout;
    selection->addWidget(new QLabel("Exigence :",this)); selection->addWidget(m_requirement,1);
    selection->addWidget(new QLabel("Profondeur :",this)); selection->addWidget(m_depth);
    QHBoxLayout *edition = new QHBoxLayout;
    edition->addWidget(new QLabel("Relation sortante :",this)); edition->addWidget(m_relationType);
    edition->addWidget(m_target,1); edition->addWidget(m_add); edition->addWidget(m_remove);

    m_relations = new QTableWidget(0,6,this);
    m_relations->setHorizontalHeaderLabels({"Sens","Type","Code","Titre","Statut","ID"});
    m_relations->setColumnHidden(5,true);
    m_relations->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_relations->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_relations->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
    m_scene = new QGraphicsScene(this);
    m_graph = new QGraphicsView(m_scene,this); m_graph->setRenderHint(QPainter::Antialiasing);
    QSplitter *splitter = new QSplitter(Qt::Vertical,this); splitter->addWidget(m_graph); splitter->addWidget(m_relations);
    splitter->setStretchFactor(0,2); splitter->setStretchFactor(1,1);
    QVBoxLayout *layout = new QVBoxLayout(this); layout->addLayout(selection); layout->addLayout(edition); layout->addWidget(splitter);

    connect(m_requirement,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&TraceabilityWidget::refreshSelection);
    connect(m_depth,QOverload<int>::of(&QSpinBox::valueChanged),this,&TraceabilityWidget::refreshSelection);
    connect(m_add,&QPushButton::clicked,this,&TraceabilityWidget::addRelation);
    connect(m_remove,&QPushButton::clicked,this,&TraceabilityWidget::removeRelation);
}

void TraceabilityWidget::setConnectionName(const QString &connectionName)
{
    m_connectionName=connectionName; refresh();
}

void TraceabilityWidget::refresh()
{
    const QSqlDatabase db=QSqlDatabase::database(m_connectionName,false);
    if(m_connectionName.isEmpty()||!db.isValid()||!db.isOpen()){
        m_requirement->clear();m_target->clear();m_relationType->clear();
        m_scene->clear();m_relations->setRowCount(0);return;
    }
    refreshRequirements(); refreshRelationTypes(); refreshSelection();
}

void TraceabilityWidget::refreshRequirements()
{
    const QVariant selected=m_requirement->currentData();
    m_requirement->blockSignals(true); m_requirement->clear(); m_target->clear();
    QSqlQuery query("SELECT ID,CODE||' — '||TITLE FROM REQUIREMENT ORDER BY CODE",QSqlDatabase::database(m_connectionName));
    while(query.next()){ m_requirement->addItem(query.value(1).toString(),query.value(0)); m_target->addItem(query.value(1).toString(),query.value(0)); }
    const int index=m_requirement->findData(selected); if(index>=0)m_requirement->setCurrentIndex(index);
    m_requirement->blockSignals(false);
}

void TraceabilityWidget::refreshRelationTypes()
{
    if(m_relationType->count())return;
    QSqlQuery query("SELECT ID,LABEL FROM REQUIREMENT_RELATION_TYPE ORDER BY ID",QSqlDatabase::database(m_connectionName));
    while(query.next())m_relationType->addItem(query.value(1).toString(),query.value(0));
}

void TraceabilityWidget::refreshSelection()
{
    if(!m_requirement->currentData().isValid()){m_scene->clear();m_relations->setRowCount(0);return;}
    const int id=m_requirement->currentData().toInt(); refreshList(id); refreshGraph(id);
}

void TraceabilityWidget::refreshList(int requirementId)
{
    m_relations->setRowCount(0);
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("SELECT X.ID,CASE WHEN X.SOURCE_REQ_ID=? THEN 'Sortant' ELSE 'Entrant' END,T.LABEL,R.CODE,R.TITLE,S.STATUS FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID JOIN REQUIREMENT R ON R.ID=CASE WHEN X.SOURCE_REQ_ID=? THEN X.TARGET_REQ_ID ELSE X.SOURCE_REQ_ID END LEFT JOIN REQ_STATUS S ON S.ID=R.STATUS WHERE X.SOURCE_REQ_ID=? OR X.TARGET_REQ_ID=? ORDER BY T.ID,R.CODE");
    query.addBindValue(requirementId);query.addBindValue(requirementId);query.addBindValue(requirementId);query.addBindValue(requirementId);query.exec();
    while(query.next()){
        const int row=m_relations->rowCount();m_relations->insertRow(row);
        for(int c=0;c<5;++c)m_relations->setItem(row,c,new QTableWidgetItem(query.value(c+1).toString()));
        m_relations->setItem(row,5,new QTableWidgetItem(query.value(0).toString()));
    }
}

void TraceabilityWidget::refreshGraph(int requirementId)
{
    m_scene->clear();
    QSqlQuery center(QSqlDatabase::database(m_connectionName));center.prepare("SELECT CODE,TITLE FROM REQUIREMENT WHERE ID=?");center.addBindValue(requirementId);center.exec();
    if(!center.next())return;
    auto addNode=[this](qreal x,qreal y,const QString &text,const QColor &color){QGraphicsRectItem *box=m_scene->addRect(x,y,220,55,QPen(color.darker()),QBrush(color));QGraphicsTextItem *label=m_scene->addText(text);label->setTextWidth(205);label->setPos(x+7,y+5);return box;};
    addNode(-110,0,center.value(0).toString()+"\n"+center.value(1).toString(),QColor("#d7e9ff"));
    const int depth=m_depth->value();
    const QString graphQuery=QString(
        "WITH RECURSIVE A(ID,LVL) AS ("
        " SELECT X.SOURCE_REQ_ID,1 FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID WHERE X.TARGET_REQ_ID=%1 AND T.CODE='DECOMPOSE'"
        " UNION ALL SELECT X.SOURCE_REQ_ID,A.LVL+1 FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID JOIN A ON X.TARGET_REQ_ID=A.ID WHERE T.CODE='DECOMPOSE' AND A.LVL<%2),"
        " D(ID,LVL) AS (SELECT X.TARGET_REQ_ID,1 FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID WHERE X.SOURCE_REQ_ID=%1 AND T.CODE='DECOMPOSE'"
        " UNION ALL SELECT X.TARGET_REQ_ID,D.LVL+1 FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID JOIN D ON X.SOURCE_REQ_ID=D.ID WHERE T.CODE='DECOMPOSE' AND D.LVL<%2),"
        " G(DIR,LVL,LABEL,ID) AS (SELECT -1,LVL,'Décompose',ID FROM A UNION SELECT 1,LVL,'Décompose',ID FROM D"
        " UNION SELECT CASE WHEN X.TARGET_REQ_ID=%1 THEN -1 ELSE 1 END,1,T.LABEL,CASE WHEN X.TARGET_REQ_ID=%1 THEN X.SOURCE_REQ_ID ELSE X.TARGET_REQ_ID END FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID WHERE T.CODE<>'DECOMPOSE' AND (X.SOURCE_REQ_ID=%1 OR X.TARGET_REQ_ID=%1))"
        " SELECT G.DIR,G.LVL,G.LABEL,R.CODE,R.TITLE FROM G JOIN REQUIREMENT R ON R.ID=G.ID ORDER BY G.DIR,G.LVL,R.CODE").arg(requirementId).arg(depth);
    QSqlQuery links(graphQuery,QSqlDatabase::database(m_connectionName));
    QMap<int,int> aboveSlots,belowSlots;
    while(links.next()){
        const bool incoming=links.value(0).toInt()<0;const int level=links.value(1).toInt();int &slot=incoming?aboveSlots[level]:belowSlots[level];const qreal x=(slot++%4-1.5)*250;const qreal y=(incoming?-1:1)*120*level;
        const QColor color=links.value(2).toString()=="Dépend de"?QColor("#ffe6b3"):QColor("#e3f3df");addNode(x-110,y,links.value(3).toString()+"\n"+links.value(4).toString(),color);
        m_scene->addLine(0,incoming?0:55,x,incoming?y+55:y,QPen(color.darker(),2));
    }
    m_graph->fitInView(m_scene->itemsBoundingRect().adjusted(-20,-20,20,20),Qt::KeepAspectRatio);
}

bool TraceabilityWidget::wouldCreateDecompositionCycle(int parentId,int childId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("WITH RECURSIVE descendants(ID) AS (SELECT TARGET_REQ_ID FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID WHERE X.SOURCE_REQ_ID=? AND T.CODE='DECOMPOSE' UNION SELECT X.TARGET_REQ_ID FROM REQUIREMENT_RELATION X JOIN REQUIREMENT_RELATION_TYPE T ON T.ID=X.TYPE_ID JOIN descendants D ON X.SOURCE_REQ_ID=D.ID WHERE T.CODE='DECOMPOSE') SELECT 1 FROM descendants WHERE ID=? LIMIT 1");
    query.addBindValue(childId);query.addBindValue(parentId);return query.exec()&&query.next();
}

void TraceabilityWidget::addRelation()
{
    const int source=m_requirement->currentData().toInt(),target=m_target->currentData().toInt(),type=m_relationType->currentData().toInt();
    if(source==target){QMessageBox::warning(this,"Relation","Une exigence ne peut pas être reliée à elle-même.");return;}
    if(type==1&&wouldCreateDecompositionCycle(source,target)){QMessageBox::warning(this,"Cycle interdit","Ce lien créerait un cycle de décomposition.");return;}
    QSqlQuery query(QSqlDatabase::database(m_connectionName));query.prepare("INSERT INTO REQUIREMENT_RELATION(SOURCE_REQ_ID,TARGET_REQ_ID,TYPE_ID) VALUES(?,?,?)");query.addBindValue(source);query.addBindValue(target);query.addBindValue(type);
    if(!query.exec()){QMessageBox::warning(this,"Relation",query.lastError().text());return;}emit dataChanged();refreshSelection();
}

void TraceabilityWidget::removeRelation()
{
    const int row=m_relations->currentRow();if(row<0)return;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));query.prepare("DELETE FROM REQUIREMENT_RELATION WHERE ID=?");query.addBindValue(m_relations->item(row,5)->text().toInt());
    if(!query.exec()){QMessageBox::warning(this,"Relation",query.lastError().text());return;}emit dataChanged();refreshSelection();
}
