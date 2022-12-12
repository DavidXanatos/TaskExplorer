######################################################################
# Qwt Examples - Copyright (C) 2002 Uwe Rathmann
# This file may be used under the terms of the 3-clause BSD License
######################################################################

QWT_ROOT = $${PWD}/..
include( $${QWT_ROOT}/qwtconfig.pri )
include( $${QWT_ROOT}/qwtbuild.pri )
include( $${QWT_ROOT}/qwtfunctions.pri )

QWT_OUT_ROOT = $${OUT_PWD}/../..

TEMPLATE     = app

INCLUDEPATH += $${QWT_ROOT}/src
DEPENDPATH  += $${QWT_ROOT}/src

INCLUDEPATH += $${QWT_ROOT}/classincludes
DEPENDPATH  += $${QWT_ROOT}/classincludes

!debug_and_release {

    DESTDIR      = $${QWT_OUT_ROOT}/playground/bin
}
else {
    CONFIG(debug, debug|release) {

        DESTDIR      = $${QWT_OUT_ROOT}/playground/bin_debug
    }
    else {

        DESTDIR      = $${QWT_OUT_ROOT}/playground/bin
    }
}


QMAKE_RPATHDIR *= $${QWT_ROOT}/lib
qwtAddLibrary($${QWT_OUT_ROOT}/lib, qwt)

greaterThan(QT_MAJOR_VERSION, 4) {

    QT += printsupport
    QT += concurrent
}   

contains(QWT_CONFIG, QwtOpenGL ) {

    QT += opengl
}
else {

    DEFINES += QWT_NO_OPENGL
}

contains(QWT_CONFIG, QwtSvg) {

    QT += svg
}
else {

    DEFINES += QWT_NO_SVG
}


contains(QWT_CONFIG, QwtDll) {
    DEFINES    += QT_DLL QWT_DLL
}
