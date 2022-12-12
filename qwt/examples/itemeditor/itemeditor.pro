######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = itemeditor

HEADERS = \
    Editor.h \
    ShapeFactory.h \
    Plot.h

SOURCES = \
    Editor.cpp \
    ShapeFactory.cpp \
    Plot.cpp \
    main.cpp

