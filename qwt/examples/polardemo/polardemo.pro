######################################################################
# Qwt Polar Examples - Copyright (C) 2008 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = polardemo

HEADERS = \
    Pixmaps.h \
    PlotBox.h \
    Plot.h \
    SettingsEditor.h

SOURCES = \
    PlotBox.cpp \
    Plot.cpp \
    SettingsEditor.cpp \
    main.cpp
