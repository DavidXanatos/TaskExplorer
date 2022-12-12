/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "CompassGrid.h"
#include "CockpitGrid.h"

#include <QApplication>
#include <QTabWidget>

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    QTabWidget tabWidget;
    tabWidget.addTab( new CompassGrid, "Compass" );
    tabWidget.addTab( new CockpitGrid, "Cockpit" );

    tabWidget.show();

    return app.exec();
}

