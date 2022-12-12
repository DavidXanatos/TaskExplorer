######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = dials

HEADERS = \
    AttitudeIndicator.h \
    SpeedoMeter.h \
    CockpitGrid.h \
    CompassGrid.h

SOURCES = \
    AttitudeIndicator.cpp \
    SpeedoMeter.cpp \
    CockpitGrid.cpp \
    CompassGrid.cpp \
    main.cpp

