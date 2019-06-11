################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

include( $${PWD}/../examples.pri )

TARGET       = event_filter

HEADERS = \
    colorbar.h \
    scalepicker.h \
    canvaspicker.h \
    plot.h 

SOURCES = \
    colorbar.cpp \
    scalepicker.cpp \
    canvaspicker.cpp \
    plot.cpp \
    event_filter.cpp 
