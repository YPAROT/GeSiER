#-------------------------------------------------
#
# Project created by QtCreator 2021-09-29T13:22:11
#
#-------------------------------------------------

QT       += core gui
QT              += sql widgets
QT              += printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = GeSiER
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    edittabledialog.cpp \
    edittableviewform.cpp \
    exportcsvdialog.cpp \
    main.cpp \
    mainwindow.cpp \
    qwidgetsettings.cpp \
    sqltableform.cpp \
    tableviewmanager.cpp \
    req_sqlmanager.cpp \
    editrequirement.cpp \
    sqltreemodel.cpp \
    sqltreeitem.cpp \
    sqltreereqmodel.cpp \
    reqtrackform.cpp \
    importrequirementdialog.cpp \
    importbasicdialog.cpp \
    importlogdialog.cpp

HEADERS += \
    edittabledialog.h \
    edittableviewform.h \
    mainwindow.h \
    qwidgetsettings.h \
    sqltableform.h \
    tableviewmanager.h \
    sqlproxyrelationaldelegate.h \
    req_sqlmanager.h \
    editrequirement.h \
    sqltreemodel.h \
    sqltreeitem.h \
    sqltreereqmodel.h \
    reqtrackform.h \
    exportcsvdialog.h \
    importrequirementdialog.h \
    importbasicdialog.h \
    importlogdialog.h

FORMS += \
    edittabledialog.ui \
    edittableviewform.ui \
    exportcsvdialog.ui \
    mainwindow.ui \
    sqltableform.ui \
    editrequirement.ui \
    reqtrackform.ui \
    importrequirementdialog.ui \
    importbasicdialog.ui \
    importlogdialog.ui

DESTDIR = $$_PRO_FILE_PWD_/bin/

QTCSV_LOCATION = $$DESTDIR
LIBS += -L$$QTCSV_LOCATION -lqtcsv
INCLUDEPATH += $$PWD/../qtcsv-master/include

message(=== Configuration of $$TARGET ===)
message(Qt version: $$[QT_VERSION])
message(binary will be created in folder: $$DESTDIR)

RESOURCES += \
    qressources.qrc

RC_ICONS =GeSiER.ico
