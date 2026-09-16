#include "historywidget.h"
#include "historyservice.h"

#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

HistoryWidget::HistoryWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  auto *filters = new QHBoxLayout;
  m_search = new QLineEdit; m_search->setPlaceholderText("Auteur, objet ou contenu…");
  m_object = new QComboBox; m_event = new QComboBox;
  auto *refreshButton = new QPushButton("Actualiser");
  auto *openButton = new QPushButton("Ouvrir l'objet");
  filters->addWidget(new QLabel("Recherche")); filters->addWidget(m_search, 1);
  filters->addWidget(new QLabel("Objet")); filters->addWidget(m_object);
  filters->addWidget(new QLabel("Événement")); filters->addWidget(m_event);
  filters->addWidget(refreshButton); filters->addWidget(openButton);
  m_table = new QTableWidget(0, 8);
  m_table->setHorizontalHeaderLabels({"Date", "Auteur", "Événement", "Objet", "ID", "Avant", "Après", "Commentaire"});
  m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_table->horizontalHeader()->setStretchLastSection(true);
  layout->addLayout(filters); layout->addWidget(m_table, 1);
  connect(refreshButton, &QPushButton::clicked, this, &HistoryWidget::refresh);
  connect(m_search, &QLineEdit::returnPressed, this, &HistoryWidget::refresh);
  connect(m_object, &QComboBox::currentTextChanged, this, [this]{ refresh(); });
  connect(m_event, &QComboBox::currentTextChanged, this, [this]{ refresh(); });
  connect(openButton, &QPushButton::clicked, this, [this]{
    const int row=m_table->currentRow(); if(row<0)return;
    emit openObjectRequested(m_table->item(row,3)->text(),m_table->item(row,4)->text().toInt());
  });
  connect(m_table, &QTableWidget::doubleClicked, openButton, &QPushButton::click);
}

void HistoryWidget::setConnectionName(const QString &connectionName) {
  m_connectionName = connectionName;
  m_object->blockSignals(true); m_event->blockSignals(true);
  m_object->clear(); m_event->clear();
  m_object->addItem("Tous", ""); m_event->addItem("Tous", "");
  if (!connectionName.isEmpty()) {
    HistoryService service(connectionName);
    for(const QString &v:service.objectTypes())m_object->addItem(v,v);
    for(const QString &v:service.eventTypes())m_event->addItem(v,v);
  }
  m_object->blockSignals(false); m_event->blockSignals(false); refresh();
}

void HistoryWidget::refresh() {
  m_table->setRowCount(0); if(m_connectionName.isEmpty())return;
  HistoryService service(m_connectionName);
  const auto rows=service.find(m_search->text(),m_object->currentData().toString(),m_event->currentData().toString());
  for(const auto&r:rows){int row=m_table->rowCount();m_table->insertRow(row);
    const QStringList values={r.time,r.author,r.eventType,r.objectType,QString::number(r.objectId),r.beforeJson,r.afterJson,r.comment};
    for(int c=0;c<values.size();++c)m_table->setItem(row,c,new QTableWidgetItem(values[c]));
  }
  m_table->resizeColumnsToContents();
}
