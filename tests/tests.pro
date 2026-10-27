QT += core gui concurrent quick testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = tst_omawrite
RESOURCES += ../src/resources.qrc

INCLUDEPATH += ../src
SOURCES += \
    tst_omawrite.cpp \
    ../src/backend.cpp \
    ../src/cli.cpp \
    ../src/markdownhighlighter.cpp \
    ../src/screenplay.cpp \
    ../src/screenplaypreview.cpp
HEADERS += \
    ../src/backend.h \
    ../src/cli.h \
    ../src/markdownhighlighter.h \
    ../src/screenplay.h \
    ../src/screenplaypreview.h

QT += widgets printsupport quickcontrols2 quickdialogs2 dbus
