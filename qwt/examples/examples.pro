######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

include( $${PWD}/../qwtconfig.pri )

TEMPLATE = subdirs

contains(QWT_CONFIG, QwtPlot) {
    
    SUBDIRS += \
        animation \
        barchart \
        cpuplot \
        curvedemo \
        distrowatch \
        friedberg \
        itemeditor \
        legends \
        stockchart \
        simpleplot \
        sinusplot \
        realtime \
        refreshtest \
        scatterplot \
        spectrogram \
        rasterview \
        tvplot 

    contains(QWT_CONFIG, QwtWidgets) {

        SUBDIRS += \
            bode \
            splineeditor \
            oscilloscope  
    }

    contains(QWT_CONFIG, QwtPolar) {

        SUBDIRS += \
            polardemo \
            polarspectrogram \
    }
}

contains(QWT_CONFIG, QwtWidgets) {

    SUBDIRS += \
        sysinfo \
        radio \
        dials \
        controls
}
