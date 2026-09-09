QT += core sql testlib
CONFIG += testcase console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = database_migrator_test
INCLUDEPATH += ..
SOURCES += database_migrator_test.cpp ../databasemigrator.cpp ../sqltreemodel.cpp ../sqltreeitem.cpp
HEADERS += ../databasemigrator.h ../sqltreemodel.h ../sqltreeitem.h
