TARGET   = httpservice
TEMPLATE = app
CONFIG   += console qt
QT = core network 

SOURCES  = main.cpp

include(../../src/qtservice.pri)
