/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QwtPlotMagnifier>
#include <QwtPlotPanner>
#include <QwtPlotPicker>
#include <QwtPickerMachine>
#include <QwtPlotCurve>
#include <QwtText>

#include <QPen>

namespace
{
    class DistancePicker : public QwtPlotPicker
    {
      public:
        DistancePicker( QWidget* canvas )
            : QwtPlotPicker( canvas )
        {
            setTrackerMode( QwtPicker::ActiveOnly );
            setStateMachine( new QwtPickerDragLineMachine() );
            setRubberBand( QwtPlotPicker::PolygonRubberBand );
        }

        virtual QwtText trackerTextF( const QPointF& pos ) const QWT_OVERRIDE
        {
            QwtText text;

            const QPolygon points = selection();
            if ( !points.isEmpty() )
            {
                QString num;
                num.setNum( QLineF( pos, invTransform( points[0] ) ).length() );

                QColor bg( Qt::white );
                bg.setAlpha( 200 );

                text.setBackgroundBrush( QBrush( bg ) );
                text.setText( num );
            }
            return text;
        }
    };
}

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
    , m_curve( NULL )
{
    canvas()->setStyleSheet(
        "border: 2px solid Black;"
        "border-radius: 15px;"
        "background-color: qlineargradient( x1: 0, y1: 0, x2: 0, y2: 1,"
            "stop: 0 LemonChiffon, stop: 1 PaleGoldenrod );"
    );

    // attach curve
    m_curve = new QwtPlotCurve( "Scattered Points" );
    m_curve->setPen( QColor( "Purple" ) );

    // when using QwtPlotCurve::ImageBuffer simple dots can be
    // rendered in parallel on multicore systems.
    m_curve->setRenderThreadCount( 0 ); // 0: use QThread::idealThreadCount()

    m_curve->attach( this );

    setSymbol( NULL );

    // panning with the left mouse button
    (void )new QwtPlotPanner( canvas() );

    // zoom in/out with the wheel
    QwtPlotMagnifier* magnifier = new QwtPlotMagnifier( canvas() );
    magnifier->setMouseButton( Qt::NoButton );

    // distanve measurement with the right mouse button
    DistancePicker* picker = new DistancePicker( canvas() );
    picker->setMousePattern( QwtPlotPicker::MouseSelect1, Qt::RightButton );
    picker->setRubberBandPen( QPen( Qt::blue ) );
}

void Plot::setSymbol( QwtSymbol* symbol )
{
    m_curve->setSymbol( symbol );

    if ( symbol == NULL )
    {
        m_curve->setStyle( QwtPlotCurve::Dots );
    }
}

void Plot::setSamples( const QVector< QPointF >& samples )
{
    m_curve->setPaintAttribute(
        QwtPlotCurve::ImageBuffer, samples.size() > 1000 );

    m_curve->setSamples( samples );
}

#include "moc_Plot.cpp"
