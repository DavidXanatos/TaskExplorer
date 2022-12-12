/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "CpuPlot.h"

#include <QApplication>
#include <QLayout>
#include <QLabel>

int main( int argc, char** argv )
{
    QApplication app( argc, argv );

    QWidget vBox;
    vBox.setWindowTitle( "Cpu Plot" );

    CpuPlot* plot = new CpuPlot( &vBox );
    plot->setTitle( "History" );

    const int margin = 5;
    plot->setContentsMargins( margin, margin, margin, margin );

    QString info( "Press the legend to en/disable a curve" );

    QLabel* label = new QLabel( info, &vBox );

    QVBoxLayout* layout = new QVBoxLayout( &vBox );
    layout->addWidget( plot );
    layout->addWidget( label );

    vBox.resize( 600, 400 );
    vBox.show();

    return app.exec();
}
