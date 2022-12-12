######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = stockchart

HEADERS = \
    Legend.h \
    GridItem.h \
    Plot.h \
    QuoteFactory.h

SOURCES = \
    Legend.cpp \
    GridItem.cpp \
    Plot.cpp \
    QuoteFactory.cpp \
    main.cpp
