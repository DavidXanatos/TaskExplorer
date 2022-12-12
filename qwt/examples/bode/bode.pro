################################################################
# Qwt Examples
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

include( $${PWD}/../examples.pri )

TARGET       = bode

HEADERS = \
    MainWindow.h \
    Plot.h \
    ComplexNumber.h \ 
    Pixmaps.h

SOURCES = \
    Plot.cpp \
    MainWindow.cpp \
    main.cpp
