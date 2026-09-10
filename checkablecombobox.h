#ifndef CHECKABLECOMBOBOX_H
#define CHECKABLECOMBOBOX_H

#include <QToolButton>

class QMenu;

class CheckableComboBox : public QToolButton {
  Q_OBJECT
public:
  explicit CheckableComboBox(const QString &title, QWidget *parent = nullptr);
  void clearItems();
  void addItem(const QString &text, int id, const QString &toolTip = {});
  QList<int> checkedIds() const;
  void setCheckedIds(const QList<int> &ids);

signals:
  void selectionChanged();

private:
  void addCommands();
  void updateSummary();
  QString m_title;
  QMenu *m_menu;
};

#endif
