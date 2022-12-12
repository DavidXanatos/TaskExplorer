/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "CanvasPicker.h"

#include <QwtPlot>
#include <QwtPlotCurve>

#include <QMouseEvent>

CanvasPicker::CanvasPicker( bool sortedX, QwtPlot* plot )
    : QObject( plot )
    , m_selectedPoint( -1 )
    , m_sortedX( sortedX )
{
    plot->canvas()->installEventFilter( this );
}

QwtPlot* CanvasPicker::plot()
{
    return qobject_cast< QwtPlot* >( parent() );
}

const QwtPlot* CanvasPicker::plot() const
{
    return qobject_cast< const QwtPlot* >( parent() );
}

bool CanvasPicker::eventFilter( QObject* object, QEvent* event )
{
    if ( plot() == NULL || object != plot()->canvas() )
        return false;

    switch( event->type() )
    {
        case QEvent::MouseButtonPress:
        {
            const QMouseEvent* mouseEvent = static_cast< QMouseEvent* >( event );
            select( mouseEvent->pos() );
            return true;
        }
        case QEvent::MouseMove:
        {
            const QMouseEvent* mouseEvent = static_cast< QMouseEvent* >( event );
            move( mouseEvent->pos() );
            return true;
        }
        default:
            break;
    }

    return QObject::eventFilter( object, event );
}

// Select the point at a position. If there is no point
// deselect the selected point

void CanvasPicker::select( const QPointF& pos )
{
    QwtPlotCurve* curve = NULL;
    double dist = 10e10;
    int index = -1;

    const QwtPlotItemList& itmList = plot()->itemList();
    for ( QwtPlotItemIterator it = itmList.begin();
        it != itmList.end(); ++it )
    {
        if ( ( *it )->rtti() == QwtPlotItem::Rtti_PlotCurve )
        {
            QwtPlotCurve* c = static_cast< QwtPlotCurve* >( *it );
            if ( c->isVisible() )
            {
                double d;
                int idx = c->closestPoint( pos, &d );
                if ( d < dist )
                {
                    curve = c;
                    index = idx;
                    dist = d;
                }
            }
        }
    }

    m_selectedCurve = NULL;
    m_selectedPoint = -1;

    if ( curve && dist < 10 ) // 10 pixels tolerance
    {
        m_selectedCurve = curve;
        m_selectedPoint = index;
    }
}

// Move the selected point
void CanvasPicker::move( const QPointF& pos )
{
    if ( m_selectedCurve == 0 || m_selectedPoint < 0  )
        return;

    QVector< double > xData( m_selectedCurve->dataSize() );
    QVector< double > yData( m_selectedCurve->dataSize() );

    double dx = 0.0;
    double dy = 0.0;

    int numPoints = static_cast< int >( m_selectedCurve->dataSize() );
    for ( int i = 0; i < numPoints; i++ )
    {
        const QPointF sample = m_selectedCurve->sample( i );

        if ( i == m_selectedPoint )
        {
            double x = plot()->invTransform(
                m_selectedCurve->xAxis(), pos.x() );
            double y = plot()->invTransform(
                m_selectedCurve->yAxis(), pos.y() );

            if ( m_sortedX )
            {
                if ( i > 0 )
                {
                    const double xMin = m_selectedCurve->sample( i - 1 ).x();
                    if ( x <= xMin )
                        x = xMin + 1;
                }

                if ( i < numPoints - 1 )
                {
                    const double xMax = m_selectedCurve->sample( i + 1 ).x();
                    if ( x >= xMax )
                        x = xMax - 1;
                }
            }

            xData[i] = x;
            yData[i] = y;

            dx = x - sample.x();
            dy = y - sample.y();
        }
        else
        {
            xData[i] = sample.x();
            yData[i] = sample.y();
        }
    }
    m_selectedCurve->setSamples( xData, yData );

    const QwtPlotItemList curves = plot()->itemList( QwtPlotItem::Rtti_PlotCurve );
    for ( int i = 0; i < curves.size(); i++ )
    {
        QwtPlotCurve* curve = static_cast< QwtPlotCurve* >( curves[i] );
        if ( curve == m_selectedCurve )
            continue;

        xData.resize( curve->dataSize() );
        yData.resize( curve->dataSize() );

        numPoints = static_cast< int >( curve->dataSize() );
        for ( int j = 0; j < numPoints; j++ )
        {
            const QPointF sample = curve->sample( j );

            xData[j] = sample.x();
            yData[j] = sample.y();

            if ( j == m_selectedPoint )
            {
                xData[j] += dx;
                yData[j] += dy;
            }
        }
        curve->setSamples( xData, yData );
    }

    plot()->replot();
}
