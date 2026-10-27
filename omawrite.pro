QT += core gui concurrent widgets printsupport qml quick quickcontrols2 quickdialogs2 dbus

CONFIG += c++17 release
TARGET = omascript
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/markdownhighlighter.h \
    src/screenplay.h \
    src/screenplaypreview.h \
    src/systemtheme.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/markdownhighlighter.cpp \
    src/screenplay.cpp \
    src/screenplaypreview.cpp \
    src/systemtheme.cpp

RESOURCES += src/resources.qrc
