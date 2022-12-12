######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = friedberg

HEADERS = \
    Plot.h \
    Friedberg2007.h

SOURCES = \
    Friedberg2007.cpp \
    Plot.cpp \
    main.cpp
