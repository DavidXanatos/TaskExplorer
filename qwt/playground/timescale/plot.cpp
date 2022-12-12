/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "Settings.h"

#include <QwtDate>
#include <QwtDateScaleDraw>
#include <QwtDateScaleEngine>
#include <QwtPlotPanner>
#include <QwtPlotMagnifier>
#include <QwtPlotGrid>
#include <QwtPlotLayout>

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
{
    setAutoFillBackground( true );
    setPalette( Qt::darkGray );
    setCanvasBackground( Qt::white );

    plotLayout()->setAlignCanvasToScales( true );

    initAxis( QwtAxis::YLeft, "Local Time", Qt::LocalTime );
    initAxis( QwtAxis::YRight,
        "Coordinated Universal Time ( UTC )", Qt::UTC );

    QwtPlotPanner* panner = new QwtPlotPanner( canvas() );
    QwtPlotMagnifier* magnifier = new QwtPlotMagnifier( canvas() );

    for ( int axis = 0; axis < QwtAxis::AxisPositions; axis++ )
    {
        const bool on = QwtAxis::isYAxis( axis );

        setAxisVisible( axis, on );
        panner->setAxisEnabled( axis, on );
        magnifier->setAxisEnabled( axis, on );
    }

    QwtPlotGrid* grid = new QwtPlotGrid();
    grid->setMajorPen( Qt::black, 0, Qt::SolidLine );
    grid->setMinorPen( Qt::gray, 0, Qt::SolidLine );
    grid->enableX( false );
    grid->enableXMin( false );
    grid->enableY( true );
    grid->enableYMin( true );

    grid->attach( this );
}

void Plot::initAxis( int axis,
    const QString& title, Qt::TimeSpec timeSpec )
{
    setAxisTitle( axis, title );

    QwtDateScaleDraw* scaleDraw = new QwtDateScaleDraw( timeSpec );
    QwtDateScaleEngine* scaleEngine = new QwtDateScaleEngine( timeSpec );

#if 0
    if ( timeSpec == Qt::LocalTime )
    {
        scaleDraw->setTimeSpec( Qt::OffsetFromUTC );
        scaleDraw->setUtcOffset( 10 );

        scaleEngine->setTimeSpec( Qt::OffsetFromUTC );
        scaleEngine->setUtcOffset( 10 );
    }
#endif
    setAxisScaleDraw( axis, scaleDraw );
    setAxisScaleEngine( axis, scaleEngine );
}

void Plot::applySettings( const Settings& settings )
{
    applyAxisSettings( QwtAxis::YLeft, settings );
    applyAxisSettings( QwtAxis::YRight, settings );

    replot();
}

void Plot::applyAxisSettings( int axis, const Settings& settings )
{
    QwtDateScaleEngine* scaleEngine =
        static_cast< QwtDateScaleEngine* >( axisScaleEngine( axis ) );

    scaleEngine->setMaxWeeks( settings.maxWeeks );
    setAxisMaxMinor( axis, settings.maxMinorSteps );
    setAxisMaxMajor( axis, settings.maxMajorSteps );


    setAxisScale( axis, QwtDate::toDouble( settings.startDateTime ),
        QwtDate::toDouble( settings.endDateTime ) );
}

#include "moc_Plot.cpp"
