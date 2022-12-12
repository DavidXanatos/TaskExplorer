######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../examples.pri )

TARGET       = cpuplot

HEADERS =  \
    CpuStat.h \
    CpuPlot.h \
    CpuPieMarker.h 

SOURCES = \
    CpuPlot.cpp \
    CpuStat.cpp \
    CpuPieMarker.cpp \
    main.cpp 
