/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "SignalData.h"

#include <QwtPlotGrid>
#include <QwtPlotLayout>
#include <QwtPlotCanvas>
#include <QwtPlotMarker>
#include <QwtPlotCurve>
#include <QwtScaleDiv>
#include <QwtScaleMap>
#include <QwtPlotDirectPainter>
#include <QwtPainter>

#include <QEvent>

namespace
{
    class Canvas : public QwtPlotCanvas
    {
      public:
        Canvas( QwtPlot* plot = NULL )
            : QwtPlotCanvas( plot )
        {
            /*
                The backing store is important, when working with widget
                overlays ( f.e rubberbands for zooming ).
                Here we don't have them and the internal
                backing store of QWidget is good enough.
             */

            setPaintAttribute( QwtPlotCanvas::BackingStore, false );
            setBorderRadius( 10 );

            if ( QwtPainter::isX11GraphicsSystem() )
            {
#if QT_VERSION < 0x050000
                /*
                    Qt::WA_PaintOutsidePaintEvent works on X11 and has a
                    nice effect on the performance.
                 */

                setAttribute( Qt::WA_PaintOutsidePaintEvent, true );
#endif

                /*
                    Disabling the backing store of Qt improves the performance
                    for the direct painter even more, but the canvas becomes
                    a native window of the window system, receiving paint events
                    for resize and expose operations. Those might be expensive
                    when there are many points and the backing store of
                    the canvas is disabled. So in this application
                    we better don't disable both backing stores.
                 */

                if ( testPaintAttribute( QwtPlotCanvas::BackingStore ) )
                {
                    setAttribute( Qt::WA_PaintOnScreen, true );
                    setAttribute( Qt::WA_NoSystemBackground, true );
                }
            }

            setupPalette();
        }

      private:
        void setupPalette()
        {
            QPalette pal = palette();

            QLinearGradient gradient;
            gradient.setCoordinateMode( QGradient::StretchToDeviceMode );
            gradient.setColorAt( 0.0, QColor( 0, 49, 110 ) );
            gradient.setColorAt( 1.0, QColor( 0, 87, 174 ) );

            pal.setBrush( QPalette::Window, QBrush( gradient ) );

            // QPalette::WindowText is used for the curve color
            pal.setColor( QPalette::WindowText, Qt::green );

            setPalette( pal );
        }
    };

    class CurveData : public QwtSeriesData< QPointF >
    {
      public:
        const SignalData& values() const
        {
            return SignalData::instance();
        }

        SignalData& values()
        {
            return SignalData::instance();
        }

        virtual QPointF sample( size_t index ) const QWT_OVERRIDE
        {
            return SignalData::instance().value( index );
        }

        virtual size_t size() const QWT_OVERRIDE
        {
            return SignalData::instance().size();
        }

        virtual QRectF boundingRect() const QWT_OVERRIDE
        {
            return SignalData::instance().boundingRect();
        }
    };
}

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
    , m_paintedPoints( 0 )
    , m_interval( 0.0, 10.0 )
    , m_timerId( -1 )
{
    m_directPainter = new QwtPlotDirectPainter();

    setAutoReplot( false );
    setCanvas( new Canvas() );

    plotLayout()->setAlignCanvasToScales( true );

    setAxisTitle( QwtAxis::XBottom, "Time [s]" );
    setAxisScale( QwtAxis::XBottom, m_interval.minValue(), m_interval.maxValue() );
    setAxisScale( QwtAxis::YLeft, -200.0, 200.0 );

    QwtPlotGrid* grid = new QwtPlotGrid();
    grid->setPen( Qt::gray, 0.0, Qt::DotLine );
    grid->enableX( true );
    grid->enableXMin( true );
    grid->enableY( true );
    grid->enableYMin( false );
    grid->attach( this );

    m_origin = new QwtPlotMarker();
    m_origin->setLineStyle( QwtPlotMarker::Cross );
    m_origin->setValue( m_interval.minValue() + m_interval.width() / 2.0, 0.0 );
    m_origin->setLinePen( Qt::gray, 0.0, Qt::DashLine );
    m_origin->attach( this );

    m_curve = new QwtPlotCurve();
    m_curve->setStyle( QwtPlotCurve::Lines );
    m_curve->setPen( canvas()->palette().color( QPalette::WindowText ) );
    m_curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
    m_curve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
    m_curve->setData( new CurveData() );
    m_curve->attach( this );
}

Plot::~Plot()
{
    delete m_directPainter;
}

void Plot::start()
{
    m_elapsedTimer.start();
    m_timerId = startTimer( 10 );
}

void Plot::replot()
{
    CurveData* curveData = static_cast< CurveData* >( m_curve->data() );
    curveData->values().lock();

    QwtPlot::replot();
    m_paintedPoints = curveData->size();

    curveData->values().unlock();
}

void Plot::setIntervalLength( double interval )
{
    if ( interval > 0.0 && interval != m_interval.width() )
    {
        m_interval.setMaxValue( m_interval.minValue() + interval );
        setAxisScale( QwtAxis::XBottom,
            m_interval.minValue(), m_interval.maxValue() );

        replot();
    }
}

void Plot::updateCurve()
{
    CurveData* curveData = static_cast< CurveData* >( m_curve->data() );
    curveData->values().lock();

    const int numPoints = curveData->size();
    if ( numPoints > m_paintedPoints )
    {
        const bool doClip = !canvas()->testAttribute( Qt::WA_PaintOnScreen );
        if ( doClip )
        {
            /*
                Depending on the platform setting a clip might be an important
                performance issue. F.e. for Qt Embedded this reduces the
                part of the backing store that has to be copied out - maybe
                to an unaccelerated frame buffer device.
             */

            const QwtScaleMap xMap = canvasMap( m_curve->xAxis() );
            const QwtScaleMap yMap = canvasMap( m_curve->yAxis() );

            QRectF br = qwtBoundingRect( *curveData,
                m_paintedPoints - 1, numPoints - 1 );

            const QRect clipRect = QwtScaleMap::transform( xMap, yMap, br ).toRect();
            m_directPainter->setClipRegion( clipRect );
        }

        m_directPainter->drawSeries( m_curve,
            m_paintedPoints - 1, numPoints - 1 );
        m_paintedPoints = numPoints;
    }

    curveData->values().unlock();
}

void Plot::incrementInterval()
{
    m_interval = QwtInterval( m_interval.maxValue(),
        m_interval.maxValue() + m_interval.width() );

    CurveData* curveData = static_cast< CurveData* >( m_curve->data() );
    curveData->values().clearStaleValues( m_interval.minValue() );

    // To avoid, that the grid is jumping, we disable
    // the autocalculation of the ticks and shift them
    // manually instead.

    QwtScaleDiv scaleDiv = axisScaleDiv( QwtAxis::XBottom );
    scaleDiv.setInterval( m_interval );

    for ( int i = 0; i < QwtScaleDiv::NTickTypes; i++ )
    {
        QList< double > ticks = scaleDiv.ticks( i );
        for ( int j = 0; j < ticks.size(); j++ )
            ticks[j] += m_interval.width();
        scaleDiv.setTicks( i, ticks );
    }
    setAxisScaleDiv( QwtAxis::XBottom, scaleDiv );

    m_origin->setValue( m_interval.minValue() + m_interval.width() / 2.0, 0.0 );

    m_paintedPoints = 0;
    replot();
}

void Plot::timerEvent( QTimerEvent* event )
{
    if ( event->timerId() == m_timerId )
    {
        updateCurve();

        const double elapsed = m_elapsedTimer.elapsed() / 1e3;
        if ( elapsed > m_interval.maxValue() )
            incrementInterval();

        return;
    }

    QwtPlot::timerEvent( event );
}

void Plot::resizeEvent( QResizeEvent* event )
{
    m_directPainter->reset();
    QwtPlot::resizeEvent( event );
}

void Plot::showEvent( QShowEvent* )
{
    replot();
}

bool Plot::eventFilter( QObject* object, QEvent* event )
{
    if ( object == canvas() &&
        event->type() == QEvent::PaletteChange )
    {
        m_curve->setPen( canvas()->palette().color( QPalette::WindowText ) );
    }

    return QwtPlot::eventFilter( object, event );
}

#include "moc_Plot.cpp"
