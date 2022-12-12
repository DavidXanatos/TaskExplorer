/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QwtColorMap>
#include <QwtPlotSpectrogram>
#include <QwtScaleWidget>
#include <QwtScaleDraw>
#include <QwtPlotZoomer>
#include <QwtPlotPanner>
#include <QwtPlotLayout>
#include <QwtPlotRenderer>
#include <QwtInterval>
#include <QwtPainter>

#include <QPrinter>
#include <QPen>
#include <QPrintDialog>
#include <QElapsedTimer>

namespace
{
    class MyZoomer : public QwtPlotZoomer
    {
      public:
        MyZoomer( QWidget* canvas )
            : QwtPlotZoomer( canvas )
        {
            setTrackerMode( AlwaysOn );
        }

        virtual QwtText trackerTextF( const QPointF& pos ) const QWT_OVERRIDE
        {
            QColor bg( Qt::white );
            bg.setAlpha( 200 );

            QwtText text = QwtPlotZoomer::trackerTextF( pos );
            text.setBackgroundBrush( QBrush( bg ) );
            return text;
        }
    };

    class SpectrogramData : public QwtRasterData
    {
      public:
        SpectrogramData()
        {
            // some minor performance improvements when the spectrogram item
            // does not need to check for NaN values

            setAttribute( QwtRasterData::WithoutGaps, true );

            m_intervals[ Qt::XAxis ] = QwtInterval( -1.5, 1.5 );
            m_intervals[ Qt::YAxis ] = QwtInterval( -1.5, 1.5 );
            m_intervals[ Qt::ZAxis ] = QwtInterval( 0.0, 10.0 );
        }

        virtual QwtInterval interval( Qt::Axis axis ) const QWT_OVERRIDE
        {
            if ( axis >= 0 && axis <= 2 )
                return m_intervals[ axis ];

            return QwtInterval();
        }

        virtual double value( double x, double y ) const QWT_OVERRIDE
        {
            const double c = 0.842;
            //const double c = 0.33;

            const double v1 = x * x + ( y - c ) * ( y + c );
            const double v2 = x * ( y + c ) + x * ( y + c );

            return 1.0 / ( v1 * v1 + v2 * v2 );
        }

      private:
        QwtInterval m_intervals[3];
    };

    class LinearColorMap : public QwtLinearColorMap
    {
      public:
        LinearColorMap( int formatType )
            : QwtLinearColorMap( Qt::darkCyan, Qt::red )
        {
            setFormat( ( QwtColorMap::Format ) formatType );

            addColorStop( 0.1, Qt::cyan );
            addColorStop( 0.6, Qt::green );
            addColorStop( 0.95, Qt::yellow );
        }
    };

    class HueColorMap : public QwtHueColorMap
    {
      public:
        HueColorMap( int formatType )
            : QwtHueColorMap( QwtColorMap::Indexed )
        {
            setFormat( ( QwtColorMap::Format ) formatType );

            //setHueInterval( 240, 60 );
            //setHueInterval( 240, 420 );
            setHueInterval( 0, 359 );
            setSaturation( 150 );
            setValue( 200 );
        }
    };

    class SaturationColorMap : public QwtSaturationValueColorMap
    {
      public:
        SaturationColorMap( int formatType )
        {
            setFormat( ( QwtColorMap::Format ) formatType );

            setHue( 220 );
            setSaturationInterval( 0, 255 );
            setValueInterval( 255, 255 );
        }
    };

    class ValueColorMap : public QwtSaturationValueColorMap
    {
      public:
        ValueColorMap( int formatType )
        {
            setFormat( ( QwtColorMap::Format ) formatType );

            setHue( 220 );
            setSaturationInterval( 255, 255 );
            setValueInterval( 70, 255 );
        }
    };

    class SVColorMap : public QwtSaturationValueColorMap
    {
      public:
        SVColorMap( int formatType )
        {
            setFormat( ( QwtColorMap::Format ) formatType );

            setHue( 220 );
            setSaturationInterval( 100, 255 );
            setValueInterval( 70, 255 );
        }
    };

    class AlphaColorMap : public QwtAlphaColorMap
    {
      public:
        AlphaColorMap( int formatType )
        {
            setFormat( ( QwtColorMap::Format ) formatType );

            //setColor( QColor("DarkSalmon") );
            setColor( QColor("SteelBlue") );
        }
    };

    class Spectrogram : public QwtPlotSpectrogram
    {
      public:
        int elapsed() const
        {
            return m_elapsed;
        }

        QSize renderedSize() const
        {
            return m_renderedSize;
        }

      protected:
        virtual QImage renderImage(
            const QwtScaleMap& xMap, const QwtScaleMap& yMap,
            const QRectF& area, const QSize& imageSize ) const QWT_OVERRIDE
        {
            QElapsedTimer t;
            t.start();

            QImage image = QwtPlotSpectrogram::renderImage(
                xMap, yMap, area, imageSize );

            m_elapsed = t.elapsed();
            m_renderedSize = imageSize;

            return image;
        }

      private:
        mutable int m_elapsed;
        mutable QSize m_renderedSize;
    };
}

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
    , m_alpha(255)
{
    m_spectrogram = new Spectrogram();
    m_spectrogram->setRenderThreadCount( 0 ); // use system specific thread count
    m_spectrogram->setCachePolicy( QwtPlotRasterItem::PaintCache );

    QList< double > contourLevels;
    for ( double level = 0.5; level < 10.0; level += 1.0 )
        contourLevels += level;
    m_spectrogram->setContourLevels( contourLevels );

    m_spectrogram->setData( new SpectrogramData() );
    m_spectrogram->attach( this );

    const QwtInterval zInterval = m_spectrogram->data()->interval( Qt::ZAxis );

    // A color bar on the right axis
    QwtScaleWidget* rightAxis = axisWidget( QwtAxis::YRight );
    rightAxis->setTitle( "Intensity" );
    rightAxis->setColorBarEnabled( true );

    setAxisScale( QwtAxis::YRight, zInterval.minValue(), zInterval.maxValue() );
    setAxisVisible( QwtAxis::YRight );

    plotLayout()->setAlignCanvasToScales( true );

    setColorMap( Plot::RGBMap );

    // LeftButton for the zooming
    // MidButton for the panning
    // RightButton: zoom out by 1
    // Ctrl+RighButton: zoom out to full size

    QwtPlotZoomer* zoomer = new MyZoomer( canvas() );
    zoomer->setMousePattern( QwtEventPattern::MouseSelect2,
        Qt::RightButton, Qt::ControlModifier );
    zoomer->setMousePattern( QwtEventPattern::MouseSelect3,
        Qt::RightButton );

    QwtPlotPanner* panner = new QwtPlotPanner( canvas() );
    panner->setAxisEnabled( QwtAxis::YRight, false );
    panner->setMouseButton( Qt::MiddleButton );

    // Avoid jumping when labels with more/less digits
    // appear/disappear when scrolling vertically

    const int extent = QwtPainter::horizontalAdvance(
        axisWidget( QwtAxis::YLeft )->fontMetrics(), "100.00" );

    axisScaleDraw( QwtAxis::YLeft )->setMinimumExtent( extent );

    const QColor c( Qt::darkBlue );
    zoomer->setRubberBandPen( c );
    zoomer->setTrackerPen( c );
}

void Plot::showContour( bool on )
{
    m_spectrogram->setDisplayMode( QwtPlotSpectrogram::ContourMode, on );
    replot();
}

void Plot::showSpectrogram( bool on )
{
    m_spectrogram->setDisplayMode( QwtPlotSpectrogram::ImageMode, on );
    m_spectrogram->setDefaultContourPen(
        on ? QPen( Qt::black, 0 ) : QPen( Qt::NoPen ) );

    replot();
}

void Plot::setColorTableSize( int type )
{
    int numColors = 0;
    switch( type )
    {
        case 1:
            numColors = 256;
            break;
        case 2:
            numColors = 1024;
            break;
        case 3:
            numColors = 16384;
            break;
    }

    m_spectrogram->setColorTableSize( numColors );
    replot();
}

void Plot::setColorMap( int type )
{
    QwtScaleWidget* axis = axisWidget( QwtAxis::YRight );
    const QwtInterval zInterval = m_spectrogram->data()->interval( Qt::ZAxis );

    m_mapType = type;

    const QwtColorMap::Format format = QwtColorMap::RGB;

    int alpha = m_alpha;
    switch( type )
    {
        case Plot::HueMap:
        {
            m_spectrogram->setColorMap( new HueColorMap( format ) );
            axis->setColorMap( zInterval, new HueColorMap( format ) );
            break;
        }
        case Plot::SaturationMap:
        {
            m_spectrogram->setColorMap( new SaturationColorMap( format ) );
            axis->setColorMap( zInterval, new SaturationColorMap( format ) );
            break;
        }
        case Plot::ValueMap:
        {
            m_spectrogram->setColorMap( new ValueColorMap( format ) );
            axis->setColorMap( zInterval, new ValueColorMap( format ) );
            break;
        }
        case Plot::SVMap:
        {
            m_spectrogram->setColorMap( new SVColorMap( format ) );
            axis->setColorMap( zInterval, new SVColorMap( format ) );
            break;
        }
        case Plot::AlphaMap:
        {
            alpha = 255;
            m_spectrogram->setColorMap( new AlphaColorMap( format ) );
            axis->setColorMap( zInterval, new AlphaColorMap( format ) );
            break;
        }
        case Plot::RGBMap:
        default:
        {
            m_spectrogram->setColorMap( new LinearColorMap( format ) );
            axis->setColorMap( zInterval, new LinearColorMap( format ) );
        }
    }
    m_spectrogram->setAlpha( alpha );

    replot();
}

void Plot::setAlpha( int alpha )
{
    // setting an alpha value doesn't make sense in combination
    // with a color map interpolating the alpha value

    m_alpha = alpha;
    if ( m_mapType != Plot::AlphaMap )
    {
        m_spectrogram->setAlpha( alpha );
        replot();
    }
}

void Plot::drawItems( QPainter* painter, const QRectF& canvasRect,
    const QwtScaleMap maps[QwtAxis::AxisPositions] ) const
{
    QwtPlot::drawItems( painter, canvasRect, maps );

    if ( m_spectrogram )
    {
        Spectrogram* spectrogram = static_cast< Spectrogram* >( m_spectrogram );

        QString info( "%1 x %2 pixels: %3 ms" );
        info = info.arg( spectrogram->renderedSize().width() );
        info = info.arg( spectrogram->renderedSize().height() );
        info = info.arg( spectrogram->elapsed() );

        Plot* plot = const_cast< Plot* >( this );
        plot->Q_EMIT rendered( info );
    }
}

#ifndef QT_NO_PRINTER

void Plot::printPlot()
{
    QPrinter printer( QPrinter::HighResolution );
#if QT_VERSION >= 0x050300
    printer.setPageOrientation( QPageLayout::Landscape );
#else
    printer.setOrientation( QPrinter::Landscape );
#endif
    printer.setOutputFileName( "spectrogram.pdf" );

    QPrintDialog dialog( &printer );
    if ( dialog.exec() )
    {
        QwtPlotRenderer renderer;

        if ( printer.colorMode() == QPrinter::GrayScale )
        {
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardBackground );
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardCanvasBackground );
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardCanvasFrame );
            renderer.setLayoutFlag( QwtPlotRenderer::FrameWithScales );
        }

        renderer.renderTo( this, printer );
    }
}

#endif

#include "moc_Plot.cpp"
