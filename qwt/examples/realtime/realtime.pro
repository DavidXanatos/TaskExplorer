######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = realtime

HEADERS = \
    ScrollZoomer.h \
    ScrollBar.h \
    IncrementalPlot.h \
    RandomPlot.h \
    MainWindow.h

SOURCES = \
    ScrollZoomer.cpp \
    ScrollBar.cpp \
    IncrementalPlot.cpp \
    RandomPlot.cpp \
    MainWindow.cpp \
    main.cpp

