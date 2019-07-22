TEMPLATE = app
CONFIG += console qt
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

SOURCES = main.cpp

include(../../src/qtservice.pri)
