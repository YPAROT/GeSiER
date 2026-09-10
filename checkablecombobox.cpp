#include "checkablecombobox.h"

#include <QAction>
#include <QMenu>
#include <QSignalBlocker>

CheckableComboBox::CheckableComboBox(const QString &title, QWidget *parent)
    : QToolButton(parent), m_title(title), m_menu(new QMenu(this)) {
  setPopupMode(QToolButton::InstantPopup);
  setMenu(m_menu);
  setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  setArrowType(Qt::DownArrow);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  addCommands();
  updateSummary();
}

void CheckableComboBox::clearItems() {
  m_menu->clear();
  addCommands();
  updateSummary();
}

void CheckableComboBox::addCommands() {
  QAction *all = m_menu->addAction("Tout sélectionner");
  QAction *none = m_menu->addAction("Tout désélectionner");
  m_menu->addSeparator();
  connect(all, &QAction::triggered, this, [this] {
    for (QAction *action : m_menu->actions())
      if (action->isCheckable())
        action->setChecked(true);
  });
  connect(none, &QAction::triggered, this, [this] {
    for (QAction *action : m_menu->actions())
      if (action->isCheckable())
        action->setChecked(false);
  });
}

void CheckableComboBox::addItem(const QString &text, int id,
                                const QString &toolTip) {
  QAction *action = m_menu->addAction(text);
  action->setCheckable(true);
  action->setData(id);
  action->setToolTip(toolTip);
  connect(action, &QAction::toggled, this, [this] {
    updateSummary();
    emit selectionChanged();
  });
  updateSummary();
}

QList<int> CheckableComboBox::checkedIds() const {
  QList<int> result;
  for (QAction *action : m_menu->actions())
    if (action->isCheckable() && action->isChecked())
      result << action->data().toInt();
  return result;
}

void CheckableComboBox::setCheckedIds(const QList<int> &ids) {
  const QSignalBlocker blocker(this);
  for (QAction *action : m_menu->actions()) {
    const QSignalBlocker actionBlocker(action);
    action->setChecked(ids.contains(action->data().toInt()));
  }
  updateSummary();
}

void CheckableComboBox::updateSummary() {
  int selected = 0;
  int total = 0;
  QStringList names;
  for (QAction *action : m_menu->actions()) {
    if (!action->isCheckable())
      continue;
    ++total;
    if (action->isChecked()) {
      ++selected;
      names << action->text().trimmed();
    }
  }
  setText(QString("%1 (%2/%3)").arg(m_title).arg(selected).arg(total));
  setToolTip(selected == 0 || selected == total ? QString("%1 : indifférent").arg(m_title)
                                                : names.join("\n"));
}
