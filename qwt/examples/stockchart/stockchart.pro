################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

include( $${PWD}/../examples.pri )

TARGET       = stockchart

HEADERS = \
    legend.h \
    griditem.h \
    plot.h \
    quotefactory.h

SOURCES = \
    legend.cpp \
    griditem.cpp \
    quotefactory.cpp \
    plot.cpp \
    main.cpp
