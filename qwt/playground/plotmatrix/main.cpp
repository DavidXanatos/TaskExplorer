/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "PlotMatrix.h"

#include <QwtPlotGrid>
#include <QwtPlot>
#include <QwtScaleWidget>
#include <QwtMath>

#include <QApplication>

namespace
{
    class MainWindow : public PlotMatrix
    {
      public:
        MainWindow();
    };
}

MainWindow::MainWindow()
    : PlotMatrix( 3, 4 )
{
    using namespace QwtAxis;

    setAxisVisible( YLeft );
    setAxisVisible( YRight );
    setAxisVisible( XBottom );

    for ( int row = 0; row < numRows(); row++ )
    {
        const double v = std::pow( 10.0, row );

        setAxisScale( YLeft, row, -v, v );
        setAxisScale( YRight, row, -v, v );
    }

    for ( int col = 0; col < numColumns(); col++ )
    {
        const double v = std::pow( 10.0, col );

        setAxisScale( XBottom, col, -v, v );
        setAxisScale( XTop, col, -v, v );
    }

    for ( int row = 0; row < numRows(); row++ )
    {
        for ( int col = 0; col < numColumns(); col++ )
        {
            QwtPlot* plot = plotAt( row, col );
            plot->setCanvasBackground( QColor( Qt::darkGray ) );

            QwtPlotGrid* grid = new QwtPlotGrid();
            grid->enableXMin( true );
            grid->setMajorPen( Qt::white, 0, Qt::DotLine );
            grid->setMinorPen( Qt::gray, 0, Qt::DotLine );
            grid->attach( plot );
        }
    }

    plotAt( 1, 0 )->axisWidget( YLeft )->setLabelRotation( 45 );
    plotAt( 1, numColumns() - 1 )->axisWidget( YRight )->setLabelRotation( -45 );

    updateLayout();
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    MainWindow window;

    window.resize( 800, 600 );
    window.show();

    return app.exec();
}
