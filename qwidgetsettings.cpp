#include "qwidgetsettings.h"
#include <QApplication>
#include <QLineEdit>
#include <QComboBox>
#include <QAbstractButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QDebug>
#include <QScrollBar>
#include <QDial>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QMetaProperty>
#include <QListWidget>
#include <QDesktopWidget>
#include <QFontComboBox>
#include <QTreeWidget>
#include <QSplitter>


QList<QWidget *> QWidgetSettings::allInputsWidgets(QWidget *parent,AllInputsTypes types,QList<QWidget*> excludedWidgets, QList<QWidget*> includedWidgets)
{
    QList<QWidget *> widgetList = parent->findChildren<QWidget *> ();
    QWidget *widget;
    for (int i= widgetList.count() - 1; i >= 0; i--)
    {
        widget = widgetList.at(i);
        QObject *object = widget;
        if (excludedWidgets.contains(widget)
                || (widget->objectName().startsWith("qt_"))
                || (widget->objectName().isEmpty())
                || (strcmp(object->metaObject()->className() , "QWidget") == 0)
                || (strcmp(object->metaObject()->className() , "QSizeGrip") == 0)
                || (strcmp(object->metaObject()->className() , "QStatusBar") == 0)
                || (!(types & QWidgetSettings::IncludeScrollBar) && (strcmp(object->metaObject()->className() , "QScrollBar") == 0))
                || (!(types & QWidgetSettings::IncludeTextAndPlainTextEdit) && (strcmp(object->metaObject()->className() , "QTextEdit") == 0))
                || (!(types & QWidgetSettings::IncludeTextAndPlainTextEdit) && (strcmp(object->metaObject()->className() , "QPlainTextEdit") == 0))
                || ((types & QWidgetSettings::DontIncludeToolAndPushButton) && (strcmp(object->metaObject()->className() , "QPushButton") == 0))
                || ((types & QWidgetSettings::DontIncludeToolAndPushButton) && (strcmp(object->metaObject()->className() , "QToolButton") == 0))
                || (!(types & QWidgetSettings::IncludeSplitter) && (strcmp(object->metaObject()->className() , "QSplitter") == 0))
                || (strcmp(object->metaObject()->className() , "QLabel") == 0))
        {
            widgetList.removeAt(i);
        }
    }
    widgetList.append(includedWidgets);

    return widgetList;
}

void QWidgetSettings::loadAllInputsSettings(QWidget *parent, AllInputsTypes types, QList<QWidget*> excludedWidgets, QList<QWidget*> includedWidgets)
{
    loadInputsSettings(parent,!(types & QWidgetSettings::DontMoveAndResizeParentWidget), allInputsWidgets(parent,types,excludedWidgets, includedWidgets));
}

void QWidgetSettings::setValue(const QString &key, const QVariant &value)
{
    QSettings settings(QApplication::applicationDirPath()+"/config/ini/"+QApplication::applicationName()+".ini",QSettings::IniFormat);
    settings.beginGroup("Custom");
    settings.setValue(key,value);
    settings.endGroup();
}

QVariant QWidgetSettings::value(const QString &key, const QVariant &defaultValue)
{
    QVariant result;
    QSettings settings(QApplication::applicationDirPath()+"/config/ini/"+QApplication::applicationName()+".ini",QSettings::IniFormat);
    settings.beginGroup("Custom");
    result = settings.value(key,defaultValue);
    settings.endGroup();

    return result;
}

QSettings*  QWidgetSettings::qSettings()
{
    return new QSettings(QApplication::applicationDirPath()+"/config/ini/"+QApplication::applicationName()+".ini",QSettings::IniFormat);
}


void QWidgetSettings::loadInputsSettings(QWidget* parent, bool restoreParentWidgetSizeAndPos, QList<QWidget*> inputsWidgets)
{
    QSettings settings(QApplication::applicationDirPath()+"/config/ini/"+QApplication::applicationName()+".ini",QSettings::IniFormat);
    qDebug() << "loading settings from "<< settings.fileName();
    if (restoreParentWidgetSizeAndPos)
    {
        settings.beginGroup("WidgetsPos");
        parent->resize(settings.value(parent->objectName()+"_size", parent->size()).toSize());
        QPoint savedPos = settings.value(parent->objectName()+"_pos", parent->pos()).toPoint();
        if ((savedPos.x() < QApplication::desktop()->width()) && (savedPos.y() < QApplication::desktop()->height()))
            parent->move(savedPos);
        settings.endGroup();
    }

    if (inputsWidgets.count() > 0)
    {
        settings.beginGroup("InputWidgets");
        settings.beginGroup(parent->objectName());

        QListIterator<QWidget*> iterWidget(inputsWidgets);
        QWidget *widget;
        while (iterWidget.hasNext())
        {
            widget = iterWidget.next();

            if (strcmp(widget->metaObject()->className() , "QLineEdit") == 0)
                dynamic_cast<QLineEdit*>(widget)->setText(settings.value(widget->objectName(),dynamic_cast<QLineEdit*>(widget)->text()).toString());
            else if ((strcmp(widget->metaObject()->className() , "QRadioButton") == 0)
                     || (strcmp(widget->metaObject()->className() , "QCheckBox") == 0)
                     || (strcmp(widget->metaObject()->className() , "QPushButton") == 0)
                     || (strcmp(widget->metaObject()->className() , "QToolButton") == 0))
                dynamic_cast<QAbstractButton*>(widget)->setChecked(settings.value(widget->objectName(),dynamic_cast<QAbstractButton*>(widget)->isChecked()).toBool());
            else if (strcmp(widget->metaObject()->className() , "QSpinBox") == 0)
                dynamic_cast<QSpinBox*>(widget)->setValue(settings.value(widget->objectName(),dynamic_cast<QSpinBox*>(widget)->value()).toInt());
            else if (strcmp(widget->metaObject()->className() , "QFontComboBox") == 0)
                dynamic_cast<QFontComboBox*>(widget)->setCurrentIndex(settings.value(widget->objectName(),dynamic_cast<QFontComboBox*>(widget)->currentIndex()).toInt());
            else if (strcmp(widget->metaObject()->className() , "QDoubleSpinBox") == 0)
                dynamic_cast<QDoubleSpinBox*>(widget)->setValue(settings.value(widget->objectName(),dynamic_cast<QDoubleSpinBox*>(widget)->value()).toDouble());
            else if (strcmp(widget->metaObject()->className() , "QTimeEdit") == 0)
                dynamic_cast<QTimeEdit*>(widget)->setTime(settings.value(widget->objectName(),dynamic_cast<QTimeEdit*>(widget)->time()).toTime());
            else if (strcmp(widget->metaObject()->className() , "QDateEdit") == 0)
                dynamic_cast<QDateEdit*>(widget)->setDate(settings.value(widget->objectName(),dynamic_cast<QDateEdit*>(widget)->dateTime()).toDate());
            else if (strcmp(widget->metaObject()->className() , "QDateTimeEdit") == 0)
                dynamic_cast<QDateTimeEdit*>(widget)->setDateTime(settings.value(widget->objectName(),dynamic_cast<QDateTimeEdit*>(widget)->dateTime()).toDateTime());
            else if (strcmp(widget->metaObject()->className() , "QComboBox") == 0)
                dynamic_cast<QComboBox*>(widget)->setCurrentIndex(settings.value(widget->objectName(),dynamic_cast<QComboBox*>(widget)->currentIndex()).toInt());
            else if (strcmp(widget->metaObject()->className() , "QSlider") == 0)
                dynamic_cast<QSlider*>(widget)->setValue(settings.value(widget->objectName(),dynamic_cast<QSlider*>(widget)->value()).toInt());
            else if (strcmp(widget->metaObject()->className() , "QScrollBar") == 0)
                dynamic_cast<QScrollBar*>(widget)->setValue(settings.value(widget->objectName(),dynamic_cast<QScrollBar*>(widget)->value()).toInt());
            else if (strcmp(widget->metaObject()->className() , "QDial") == 0)
                dynamic_cast<QDial*>(widget)->setValue(settings.value(widget->objectName(),dynamic_cast<QDial*>(widget)->value()).toInt());
            else if (strcmp(widget->metaObject()->className() , "QTextEdit") == 0)
                dynamic_cast<QTextEdit*>(widget)->setPlainText(settings.value(widget->objectName(),dynamic_cast<QTextEdit*>(widget)->toPlainText()).toString());
            else if (strcmp(widget->metaObject()->className() , "QPlainTextEdit") == 0)
                dynamic_cast<QPlainTextEdit*>(widget)->setPlainText(settings.value(widget->objectName(),dynamic_cast<QPlainTextEdit*>(widget)->toPlainText()).toString());
            else if (strcmp(widget->metaObject()->className() , "QSplitter") == 0)
                dynamic_cast<QSplitter*>(widget)->restoreState(settings.value(widget->objectName(),dynamic_cast<QSplitter*>(widget)->saveState()).toByteArray());
            else if (strcmp(widget->metaObject()->className() , "QTreeWidget") == 0)
            {
                qDebug() << "resize QTreeWidget" << dynamic_cast<QTreeWidget*>(widget)->columnCount();
                for (int i=0; i < dynamic_cast<QTreeWidget*>(widget)->columnCount();i++)
                {
                    dynamic_cast<QTreeWidget*>(widget)->setColumnWidth(i, settings.value(widget->objectName()+"_c"+QString::number(i),dynamic_cast<QTreeWidget*>(widget)->columnWidth(i)).toInt());
                    qDebug() << i <<widget->objectName()+"_c"+QString::number(i)<< settings.value(widget->objectName()+"_c"+QString::number(i)).toInt() << dynamic_cast<QTreeWidget*>(widget)->columnWidth(i);
                }
            }

        }
        settings.endGroup();
        settings.endGroup();
    }
}

void QWidgetSettings::saveAllInputsSettings(QWidget* parent,AllInputsTypes types, QList<QWidget*> excludedWidgets, QList<QWidget*> includedWidgets)
{
    saveInputsSettings(parent, allInputsWidgets(parent,types,excludedWidgets,includedWidgets));
}

void QWidgetSettings::saveInputsSettings(QWidget* parent, QList<QWidget*> inputsWidgets)
{
    QSettings settings(QApplication::applicationDirPath()+"/config/ini/"+QApplication::applicationName()+".ini",QSettings::IniFormat);
    qDebug() << "saving settings to "<< settings.fileName();
    settings.beginGroup("WidgetsPos");
    settings.setValue(parent->objectName()+"_size", parent->size());
    settings.setValue(parent->objectName()+"_pos", parent->pos());
    settings.endGroup();

    if (inputsWidgets.count() > 0)
    {
        settings.beginGroup("InputWidgets");
        settings.beginGroup(parent->objectName());
        settings.remove("");
        QListIterator<QWidget*> iterWidget(inputsWidgets);
        QWidget *widget;
        while (iterWidget.hasNext())
        {
            widget = iterWidget.next();
            if (strcmp(widget->metaObject()->className() , "QLineEdit") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QLineEdit*>(widget)->text());
            else if ((strcmp(widget->metaObject()->className() , "QRadioButton") == 0)
                     || (strcmp(widget->metaObject()->className() , "QCheckBox") == 0)
                     || (strcmp(widget->metaObject()->className() , "QPushButton") == 0)
                     || (strcmp(widget->metaObject()->className() , "QToolButton") == 0))
                settings.setValue(widget->objectName(),dynamic_cast<QAbstractButton*>(widget)->isChecked());
            else if (strcmp(widget->metaObject()->className() , "QSpinBox") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QSpinBox*>(widget)->value());
            else if (strcmp(widget->metaObject()->className() , "QFontComboBox") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QFontComboBox*>(widget)->currentIndex());
            else if (strcmp(widget->metaObject()->className() , "QDoubleSpinBox") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QDoubleSpinBox*>(widget)->value());
            else if (strcmp(widget->metaObject()->className() , "QTimeEdit") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QTimeEdit*>(widget)->time());
            else if (strcmp(widget->metaObject()->className() , "QDateEdit") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QDateEdit*>(widget)->dateTime());
            else if (strcmp(widget->metaObject()->className() , "QDateTimeEdit") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QDateTimeEdit*>(widget)->dateTime());
            else if (strcmp(widget->metaObject()->className() , "QComboBox") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QComboBox*>(widget)->currentIndex());
            else if (strcmp(widget->metaObject()->className() , "QSlider") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QSlider*>(widget)->value());
            else if (strcmp(widget->metaObject()->className() , "QScrollBar") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QScrollBar*>(widget)->value());
            else if (strcmp(widget->metaObject()->className() , "QDial") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QDial*>(widget)->value());
            else if (strcmp(widget->metaObject()->className() , "QTextEdit") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QTextEdit*>(widget)->toPlainText());
            else if (strcmp(widget->metaObject()->className() , "QPlainTextEdit") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QPlainTextEdit*>(widget)->toPlainText());
            else if (strcmp(widget->metaObject()->className() , "QSplitter") == 0)
                settings.setValue(widget->objectName(),dynamic_cast<QSplitter*>(widget)->saveState());
            else if (strcmp(widget->metaObject()->className() , "QTreeWidget") == 0)
            {
                for (int i=0; i < dynamic_cast<QTreeWidget*>(widget)->columnCount();i++)
                    settings.setValue(widget->objectName()+"_c"+QString::number(i),dynamic_cast<QTreeWidget*>(widget)->columnWidth(i));
            }

        }
        settings.endGroup();
        settings.endGroup();
    }
}
