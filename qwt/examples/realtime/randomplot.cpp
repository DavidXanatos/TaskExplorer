/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "RandomPlot.h"
#include "ScrollZoomer.h"

#include <QwtPlotGrid>
#include <QwtPlotCanvas>
#include <QwtPlotLayout>
#include <QwtScaleWidget>
#include <QwtScaleDraw>
#include <QwtMath>

#include <QPen>
#include <QTimer>

namespace
{
    const unsigned int c_rangeMax = 1000;

    class Zoomer : public ScrollZoomer
    {
      public:
        Zoomer( QWidget* canvas )
            : ScrollZoomer( canvas )
        {
            setRubberBandPen( QPen( Qt::red ) );
        }

        virtual QwtText trackerTextF( const QPointF& pos ) const QWT_OVERRIDE
        {
            QColor bg( Qt::white );

            QwtText text = QwtPlotZoomer::trackerTextF( pos );
            text.setBackgroundBrush( QBrush( bg ) );
            return text;
        }

        virtual void rescale() QWT_OVERRIDE
        {
            QwtScaleWidget* scaleWidget = plot()->axisWidget( yAxis() );
            QwtScaleDraw* sd = scaleWidget->scaleDraw();

            double minExtent = 0.0;
            if ( zoomRectIndex() > 0 )
            {
                // When scrolling in vertical direction
                // the plot is jumping in horizontal direction
                // because of the different widths of the labels
                // So we better use a fixed extent.

                minExtent = sd->spacing() + sd->maxTickLength() + 1;
                minExtent += sd->labelSize(
                    scaleWidget->font(), c_rangeMax ).width();
            }

            sd->setMinimumExtent( minExtent );

            ScrollZoomer::rescale();
        }
    };
}

RandomPlot::RandomPlot( QWidget* parent )
    : IncrementalPlot( parent )
    , m_timer( 0 )
    , m_timerCount( 0 )
{
    setFrameStyle( QFrame::NoFrame );
    setLineWidth( 0 );

    plotLayout()->setAlignCanvasToScales( true );

    QwtPlotGrid* grid = new QwtPlotGrid;
    grid->setMajorPen( Qt::gray, 0, Qt::DotLine );
    grid->attach( this );

    setCanvasBackground( QColor( 29, 100, 141 ) ); // nice blue

    setAxisScale( QwtAxis::XBottom, 0, c_rangeMax );
    setAxisScale( QwtAxis::YLeft, 0, c_rangeMax );

    replot();

    // enable zooming

    ( void ) new Zoomer( canvas() );
}

QSize RandomPlot::sizeHint() const
{
    return QSize( 540, 400 );
}

void RandomPlot::appendPoint()
{
    double x = qwtRand() % c_rangeMax;
    x += ( qwtRand() % 100 ) / 100;

    double y = qwtRand() % c_rangeMax;
    y += ( qwtRand() % 100 ) / 100;

    IncrementalPlot::appendPoint( QPointF( x, y ) );

    if ( --m_timerCount <= 0 )
        stop();
}

void RandomPlot::append( int timeout, int count )
{
    if ( !m_timer )
    {
        m_timer = new QTimer( this );
        connect( m_timer, SIGNAL(timeout()), SLOT(appendPoint()) );
    }

    m_timerCount = count;

    Q_EMIT running( true );
    m_timeStamp.start();

    QwtPlotCanvas* plotCanvas = qobject_cast< QwtPlotCanvas* >( canvas() );
    plotCanvas->setPaintAttribute( QwtPlotCanvas::BackingStore, false );

    m_timer->start( timeout );
}

void RandomPlot::stop()
{
    Q_EMIT elapsed( m_timeStamp.elapsed() );

    if ( m_timer )
    {
        m_timer->stop();
        Q_EMIT running( false );
    }

    QwtPlotCanvas* plotCanvas = qobject_cast< QwtPlotCanvas* >( canvas() );
    plotCanvas->setPaintAttribute( QwtPlotCanvas::BackingStore, true );
}

void RandomPlot::clear()
{
    clearPoints();
    replot();
}

#include "moc_RandomPlot.cpp"
