QT += core sql testlib
CONFIG += testcase console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = database_migrator_test
INCLUDEPATH += ..
SOURCES += database_migrator_test.cpp ../databasemigrator.cpp
HEADERS += ../databasemigrator.h
