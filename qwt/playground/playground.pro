######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../qwtconfig.pri )

TEMPLATE = subdirs

contains(QWT_CONFIG, QwtPlot) {
    
    SUBDIRS += \
        plotmatrix \
        timescale \
        scaleengine \
        rescaler \
        shapes \
        curvetracker \
        vectorfield \
        symbols

    greaterThan(QT_MAJOR_VERSION, 4) {
        qtHaveModule(svg) {
            # we only need SVG support from Qt, but not from Qwt
            CONFIG += svgexamples
        }
    }
    else {
        contains(QWT_CONFIG, QwtSvg) {
            CONFIG += svgexamples
        }
    }

    svgexamples {
        SUBDIRS += \
            svgmap \
            graphicscale
    }
}
