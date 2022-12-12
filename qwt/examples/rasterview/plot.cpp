/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QwtColorMap>
#include <QwtPlotSpectrogram>
#include <QwtPlotLayout>
#include <QwtMatrixRasterData>
#include <QwtScaleWidget>
#include <QwtPlotMagnifier>
#include <QwtPlotPanner>
#include <QwtPlotRenderer>
#include <QwtPlotGrid>
#include <QwtPlotCanvas>
#include <QwtInterval>

namespace
{
    class RasterData : public QwtMatrixRasterData
    {
      public:
        RasterData()
        {
            const double matrix[] =
            {
                1, 2, 4, 1,
                6, 3, 5, 2,
                4, 2, 1, 5,
                5, 4, 2, 3
            };

            QVector< double > values;
            for ( uint i = 0; i < sizeof( matrix ) / sizeof( double ); i++ )
                values += matrix[i];

            const int numColumns = 4;
            setValueMatrix( values, numColumns );

            setInterval( Qt::XAxis,
                QwtInterval( -0.5, 3.5, QwtInterval::ExcludeMaximum ) );
            setInterval( Qt::YAxis,
                QwtInterval( -0.5, 3.5, QwtInterval::ExcludeMaximum ) );
            setInterval( Qt::ZAxis, QwtInterval( 1.0, 6.0 ) );
        }
    };

    class ColorMap : public QwtLinearColorMap
    {
      public:
        ColorMap()
            : QwtLinearColorMap( Qt::darkBlue, Qt::darkRed )
        {
            addColorStop( 0.2, Qt::blue );
            addColorStop( 0.4, Qt::cyan );
            addColorStop( 0.6, Qt::yellow );
            addColorStop( 0.8, Qt::red );
        }
    };
}

Plot::Plot( QWidget* parent ):
    QwtPlot( parent )
{
    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setBorderRadius( 10 );
    setCanvas( canvas );

#if 0
    QwtPlotGrid* grid = new QwtPlotGrid();
    grid->setPen( Qt::DotLine );
    grid->attach( this );
#endif

    m_spectrogram = new QwtPlotSpectrogram();
    m_spectrogram->setRenderThreadCount( 0 ); // use system specific thread count

    m_spectrogram->setColorMap( new ColorMap() );

    m_spectrogram->setData( new RasterData() );
    m_spectrogram->attach( this );

    const QwtInterval zInterval = m_spectrogram->data()->interval( Qt::ZAxis );
    // A color bar on the right axis
    QwtScaleWidget* rightAxis = axisWidget( QwtAxis::YRight );
    rightAxis->setColorBarEnabled( true );
    rightAxis->setColorBarWidth( 40 );
    rightAxis->setColorMap( zInterval, new ColorMap() );

    setAxisScale( QwtAxis::YRight, zInterval.minValue(), zInterval.maxValue() );
    setAxisVisible( QwtAxis::YRight );

    plotLayout()->setAlignCanvasToScales( true );

    setAxisScale( QwtAxis::XBottom, 0.0, 3.0 );
    setAxisMaxMinor( QwtAxis::XBottom, 0 );
    setAxisScale( QwtAxis::YLeft, 0.0, 3.0 );
    setAxisMaxMinor( QwtAxis::YLeft, 0 );

    QwtPlotMagnifier* magnifier = new QwtPlotMagnifier( canvas );
    magnifier->setAxisEnabled( QwtAxis::YRight, false );

    QwtPlotPanner* panner = new QwtPlotPanner( canvas );
    panner->setAxisEnabled( QwtAxis::YRight, false );
}

void Plot::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "rasterview.pdf" );
}

void Plot::setResampleMode( int mode )
{
    RasterData* rasterData = static_cast< RasterData* >( m_spectrogram->data() );
    rasterData->setResampleMode(
        static_cast< QwtMatrixRasterData::ResampleMode >( mode ) );

    replot();
}

#include "moc_Plot.cpp"
