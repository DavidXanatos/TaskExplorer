######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = refreshtest

HEADERS = \
    Settings.h \
    CircularBuffer.h \
    Panel.h \
    Plot.h \
    MainWindow.h

SOURCES = \
    CircularBuffer.cpp \
    Panel.cpp \
    Plot.cpp \
    MainWindow.cpp \
    main.cpp

