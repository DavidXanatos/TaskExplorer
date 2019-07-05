
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += core gui
TEMPLATE = lib

VERSION = 4.1.0

DEFINES += QHEXEDIT_EXPORTS

HEADERS = \
    qhexedit.h \
    chunks.h \
    commands.h


SOURCES = \
    qhexedit.cpp \
    chunks.cpp \
    commands.cpp

Release:TARGET = qhexedit
Debug:TARGET = qhexeditd


unix:DESTDIR = /usr/lib
win32:DESTDIR = ../lib
