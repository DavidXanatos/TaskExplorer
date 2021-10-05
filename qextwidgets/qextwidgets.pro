greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += core gui
TEMPLATE = lib

# VERSION = 4.1.0

DEFINES += QT_WIDGETS_LIB QEXTWIDGETS_LIB

HEADERS += ./qtabwidgetex_p.h \
    ./qextwidgets_global.h \
    ./qtabbarex.h \
    ./qtabbarex_p.h \
    ./qtabwidgetex.h
SOURCES += ./qtabbarex.cpp \
    ./qtabwidgetex.cpp


Release:TARGET = qextwidgets
Debug:TARGET = qextwidgetsd


unix:DESTDIR = /usr/lib
win32:DESTDIR = ../lib









