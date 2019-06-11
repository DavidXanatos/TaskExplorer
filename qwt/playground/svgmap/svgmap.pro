################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

include( $${PWD}/../playground.pri )

!contains(QWT_CONFIG, QwtSvg) {

    message(Are you trying to build Qwt with the Qt Creator as Shadow Build ?)
    error(Qwt is configured without SVG support !)
}

TARGET   = svgmap
QT      += svg

HEADERS = \
    plot.h

SOURCES = \
    plot.cpp \
    main.cpp
