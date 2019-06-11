################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

QWT_ROOT = $${PWD}/..
QWT_OUT_ROOT = $${OUT_PWD}/..

include ( $${QWT_ROOT}/qwtconfig.pri )
include ( $${QWT_ROOT}/qwtbuild.pri )
include ( $${QWT_ROOT}/qwtfunctions.pri )


CONFIG( debug_and_release ) {

    # When building debug_and_release the designer plugin is built
    # for release only. If you want to have a debug version it has to be
    # done with "CONFIG += debug" only.

    message("debug_and_release: building the Qwt designer plugin in release mode only")

    CONFIG -= debug_and_release
    CONFIG += release
}

contains(QWT_CONFIG, QwtDesigner) {

    CONFIG    += qt plugin 

    greaterThan(QT_MAJOR_VERSION, 4) {

        QT += designer
    }
    else {

        CONFIG    += designer
    }


    TEMPLATE        = lib
    TARGET          = qwt_designer_plugin

    DESTDIR         = plugins/designer

    INCLUDEPATH    += $${QWT_ROOT}/src 
    DEPENDPATH     += $${QWT_ROOT}/src 

    contains(QWT_CONFIG, QwtDll) {

        contains(QWT_CONFIG, QwtDesignerSelfContained) {
            QWT_CONFIG += include_src
        }

    } else {

        # for linking against a static library the 
        # plugin will be self contained anyway 
    }

    contains(QWT_CONFIG, include_src) {

        # compile all qwt classes into the plugin

        include ( $${QWT_ROOT}/src/src.pri )

        for( header, HEADERS) {
            QWT_HEADERS += $${QWT_ROOT}/src/$${header}
        }

        for( source, SOURCES ) {
            QWT_SOURCES += $${QWT_ROOT}/src/$${source}
        }

        HEADERS = $${QWT_HEADERS}
        SOURCES = $${QWT_SOURCES}

    } else {

        # compile the path for finding the Qwt library
        # into the plugin. Not supported on Windows !

        QMAKE_RPATHDIR *= $${QWT_INSTALL_LIBS}
        qwtAddLibrary($${QWT_OUT_ROOT}/lib, qwt)

        contains(QWT_CONFIG, QwtDll) {

            win32 {
                DEFINES += QT_DLL QWT_DLL
            }
        }
    }

    !contains(QWT_CONFIG, QwtPlot) {
        DEFINES += NO_QWT_PLOT
    }

    !contains(QWT_CONFIG, QwtWidgets) {
        DEFINES += NO_QWT_WIDGETS
    }

    HEADERS += qwt_designer_plugin.h
    SOURCES += qwt_designer_plugin.cpp

    contains(QWT_CONFIG, QwtPlot) {

        HEADERS += qwt_designer_plotdialog.h
        SOURCES += qwt_designer_plotdialog.cpp
    }

    RESOURCES += qwt_designer_plugin.qrc

    target.path = $${QWT_INSTALL_PLUGINS}
    INSTALLS += target
}
else {
    TEMPLATE        = subdirs # do nothing
}
