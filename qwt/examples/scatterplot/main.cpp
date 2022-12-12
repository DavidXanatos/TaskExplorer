/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QwtMath>
#include <QApplication>

static inline double randomValue()
{
    // a number between [ 0.0, 1.0 ]
    return ( qwtRand() % 100000 ) / 100000.0;
}

class ScatterPlot : public Plot
{
  public:
    ScatterPlot()
    {
        setTitle( "Scatter Plot" );

        // a million points

        const int numPoints = 100000;

        QPolygonF samples;
        samples.reserve( numPoints );

        for ( int i = 0; i < numPoints; i++ )
        {
            const double x = randomValue() * 24.0 + 1.0;
            const double y = std::log( 10.0 * ( x - 1.0 ) + 1.0 )
                * ( randomValue() * 0.5 + 0.9 );

            samples += QPointF( x, y );
        }

        setSamples( samples );
    }
};

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    ScatterPlot plot;
    plot.resize( 800, 600 );
    plot.show();

    return app.exec();
}
