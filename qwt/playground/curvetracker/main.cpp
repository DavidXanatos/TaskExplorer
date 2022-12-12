/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include <QApplication>

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    Plot plot;
    plot.resize( 600, 400 );
    plot.show();

    return app.exec();
}
