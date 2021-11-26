#ifndef QWIDGETSETTINGS_H
#define QWIDGETSETTINGS_H

#include <QList>
#include <QWidget>
#include <QHash>
#include <QSettings>
//*************************************************************************
//
//  Sauvegarde et rechargement des parametres d'une fenetre
//  Les widgets et leur propriété pris en compte sont les suivant :
//                              QLineEdit -> text
//                              QRadioButton,QCheckBox -> checked
//                              QPushButton,QToolButton -> checked (non pris en compte si utilisation du flag DontIncludeToolAndPushButton)
//                              QSpinBox,QDoubleSpinBox -> value
//                              QTimeEdit -> time
//                              QDateEdit -> date
//                              DateTimeEdit -> dateTime
//                              QComboBox -> currentIndex
//                              QDial,QSlider -> value
//                              QFontComboBox -> index
//                              QScrollBar -> value
//                              QTextEdit,QPlainTextEdit -> plainText
//                              QSplitter -> saveState
//
//
//  Dans le cas des loadAllInputsSettings et saveAllInputsSettings, les widgets pris en compte dependent du flag AllInputsType:
//      - DefaultInputsTypes      :
//                              QLineEdit -> text
//                              QRadioButton,QCheckBox -> checked
//                              QPushButton,QToolButton -> checked (non pris en compte si utilisation du flag DontIncludeToolAndPushButton)
//                              QSpinBox,QDoubleSpinBox -> value
//                              QTimeEdit -> time
//                              QDateEdit -> date
//                              DateTimeEdit -> dateTime
//                              QComboBox -> currentIndex
//                              QDial,QSlider -> value
//                              QFontComboBox -> index
//       -IncludeScrollBar   :
//                              QScrollBar -> value
//       -IncludeTextAndPlainTextEdit :
//                              QTextEdit,QPlainTextEdit -> plainText
//       -IncludeSplitter :
//                              QSplitter -> saveState
// L'usage de loadAllInputsSettings et saveAllInputsSettings permet de selectionner l'ensemble des inputs d'une fenetre et d'en exclure certains et d'en rajouter d'autres.
// exemples :
// QWidgetSettings::loadAllInputsSettings(this); => charge tout les inputs par defaut
// QWidgetSettings::loadAllInputsSettings(this, QWidgetSettings::IncludeScrollBar | QWidgetSettings::DontMoveAndResizeParentWidget) => rajoute la prise en charge des scrollBars et ne modifie pas la position et taille de la fenetre parent
// QWidgetSettings::loadAllInputsSettings(this, QWidgetSettings::DefaultInputsTypes, QList<QWidget*>() << ui->lineEdit_3); => ne recharge pas le texte de ui->lineEdit_3
// QWidgetSettings::loadAllInputsSettings(this, QWidgetSettings::DefaultInputsTypes, QList<QWidget*>(), QList<QWidget*>() << ui->specialTextEdit); => rajoute dans la liste a prendre en compte un textEdit (parmis ceux du widget parent)
// - Attention : les saveAllInputsSettings() doivent prendre en argument les memes argument que le loadAllInputsSettings pour que cela marche ...
//
// usage en custom (utilisation de loadInputsSettings et saveInputsSettings)
// QWidgetSettings::loadInputsSettings(this,true, QList<QWidget*>() << ui->lineEdit_3 << ui->radioButton_1 << ui->monComboBox);
// ...
// QWidgetSettings::saveInputsSettings(this, QList<QWidget*>() << ui->lineEdit_3 << ui->radioButton_1 << ui->monComboBox);
//*************************************************************************
class QWidgetSettings : public QObject
{
    Q_OBJECT
public:
    QWidgetSettings(){}

    enum AllInputsType {
        DefaultInputsTypes              = 0x00000000,
        DontMoveAndResizeParentWidget   = 0x00000001,
        IncludeScrollBar                = 0x00000002,
        IncludeTextAndPlainTextEdit     = 0x00000004,
        DontIncludeToolAndPushButton    = 0x00000008,
        IncludeSplitter                 = 0x00000010
    };

    Q_DECLARE_FLAGS(AllInputsTypes, AllInputsType)

    static QList<QWidget *> allInputsWidgets(QWidget *parent,AllInputsTypes types = DefaultInputsTypes,QList<QWidget*> excludedWidgets = QList<QWidget*>(),QList<QWidget*> includedWidgets = QList<QWidget*>());

    static void loadAllInputsSettings(QWidget* parent, AllInputsTypes types = DefaultInputsTypes, QList<QWidget*> excludedWidgets = QList<QWidget*>(),QList<QWidget*> includedWidgets = QList<QWidget*>());
    static void saveAllInputsSettings(QWidget* parent, AllInputsTypes types = DefaultInputsTypes, QList<QWidget*> excludedWidgets = QList<QWidget*>(),QList<QWidget*> includedWidgets = QList<QWidget*>());

    static void loadInputsSettings(QWidget* parent, bool restoreParentWidgetSizeAndPos, QList<QWidget*> inputsWidgets);
    static void saveInputsSettings(QWidget* parent, QList<QWidget*> inputsWidgets);

    static void loadAllInputsSettings(QWidget* parent,QString filename, AllInputsTypes types = DefaultInputsTypes, QList<QWidget*> excludedWidgets = QList<QWidget*>(),QList<QWidget*> includedWidgets = QList<QWidget*>());
    static void saveAllInputsSettings(QWidget* parent,QString filename, AllInputsTypes types = DefaultInputsTypes, QList<QWidget*> excludedWidgets = QList<QWidget*>(),QList<QWidget*> includedWidgets = QList<QWidget*>());

    static void loadInputsSettings(QWidget* parent,QString filename, bool restoreParentWidgetSizeAndPos, QList<QWidget*> inputsWidgets);
    static void saveInputsSettings(QWidget* parent,QString filename, QList<QWidget*> inputsWidgets);

    static void setValue(const QString &key, const QVariant &value);
    static QVariant value(const QString &key, const QVariant &defaultValue = QVariant());
    static QSettings* qSettings();


};
Q_DECLARE_OPERATORS_FOR_FLAGS(QWidgetSettings::AllInputsTypes)
#endif // QWIDGETSETTINGS_H
