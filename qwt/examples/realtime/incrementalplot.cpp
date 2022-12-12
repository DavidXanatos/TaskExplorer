/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "IncrementalPlot.h"

#include <QwtPlot>
#include <QwtPlotCurve>
#include <QwtSymbol>
#include <QwtScaleMap>
#include <QwtPlotDirectPainter>
#include <QwtPainter>

namespace
{
    class CurveData : public QwtArraySeriesData< QPointF >
    {
      public:
        virtual QRectF boundingRect() const QWT_OVERRIDE
        {
            if ( cachedBoundingRect.width() < 0.0 )
                cachedBoundingRect = qwtBoundingRect( *this );

            return cachedBoundingRect;
        }

        inline void append( const QPointF& point )
        {
            m_samples += point;
        }

        void clear()
        {
            m_samples.clear();
            m_samples.squeeze();
            cachedBoundingRect = QRectF( 0.0, 0.0, -1.0, -1.0 );
        }
    };
}

IncrementalPlot::IncrementalPlot( QWidget* parent )
    : QwtPlot( parent )
    , m_curve( NULL )
{
    m_directPainter = new QwtPlotDirectPainter( this );

    if ( QwtPainter::isX11GraphicsSystem() )
    {
#if QT_VERSION < 0x050000
        canvas()->setAttribute( Qt::WA_PaintOutsidePaintEvent, true );
#endif
        canvas()->setAttribute( Qt::WA_PaintOnScreen, true );
    }

    m_curve = new QwtPlotCurve( "Test Curve" );
    m_curve->setData( new CurveData() );
    showSymbols( true );

    m_curve->attach( this );

    setAutoReplot( false );
}

IncrementalPlot::~IncrementalPlot()
{
    delete m_curve;
}

void IncrementalPlot::appendPoint( const QPointF& point )
{
    CurveData* curveData = static_cast< CurveData* >( m_curve->data() );
    curveData->append( point );

    const bool doClip = !canvas()->testAttribute( Qt::WA_PaintOnScreen );
    if ( doClip && m_curve->symbol() )
    {
        /*
           Depending on the platform setting a clip might be an important
           performance issue. F.e. for Qt Embedded this reduces the
           part of the backing store that has to be copied out - maybe
           to an unaccelerated frame buffer device.
         */
        const QwtScaleMap xMap = canvasMap( m_curve->xAxis() );
        const QwtScaleMap yMap = canvasMap( m_curve->yAxis() );

        const QSize symbolSize = m_curve->symbol()->size();
        QRect r( 0, 0, symbolSize.width() + 2, symbolSize.height() + 2 );

        const QPointF center = QwtScaleMap::transform( xMap, yMap, point );
        r.moveCenter( center.toPoint() );

        m_directPainter->setClipRegion( r );
    }

    m_directPainter->drawSeries( m_curve,
        curveData->size() - 1, curveData->size() - 1 );
}

void IncrementalPlot::clearPoints()
{
    CurveData* curveData = static_cast< CurveData* >( m_curve->data() );
    curveData->clear();

    replot();
}

void IncrementalPlot::showSymbols( bool on )
{
    if ( on )
    {
        m_curve->setStyle( QwtPlotCurve::NoCurve );
        m_curve->setSymbol( new QwtSymbol( QwtSymbol::XCross,
            Qt::NoBrush, QPen( Qt::white ), QSize( 4, 4 ) ) );
    }
    else
    {
        m_curve->setPen( Qt::white );
        m_curve->setStyle( QwtPlotCurve::Dots );
        m_curve->setSymbol( NULL );
    }

    replot();
}

#include "moc_IncrementalPlot.cpp"
