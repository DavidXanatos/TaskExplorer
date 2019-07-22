
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += core gui
TEMPLATE = lib

VERSION = 4.1.0

DEFINES += QHEXEDIT_EXPORTS

HEADERS = \
    qhexedit.h \
    chunks.h \
    commands.h \
    ../editor/HexEditor.h \
    ../editor/HexEditorOptions.h \
    ../editor/HexEditorSearch.h

SOURCES = \
    qhexedit.cpp \
    chunks.cpp \
    commands.cpp \
    ../editor/HexEditor.cpp \
    ../editor/HexEditorOptions.cpp \
    ../editor/HexEditorSearch.cpp

RESOURCES = \
    ../editor/Resource/qhexedit.qrc

FORMS += \
    ../editor/Resource/HexEditorOptions.ui \
    ../editor/Resource/HexEditorSearch.ui

Release:TARGET = qhexedit
Debug:TARGET = qhexeditd


unix:DESTDIR = /usr/lib
win32:DESTDIR = ../lib
