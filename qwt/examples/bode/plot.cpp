/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "Plot.h"
#include "ComplexNumber.h"

#include <QwtMath>
#include <QwtScaleEngine>
#include <QwtSymbol>
#include <QwtPlotGrid>
#include <QwtPlotMarker>
#include <QwtPlotCurve>
#include <QwtLegend>
#include <QwtText>
#include <QwtPlotCanvas>

static void logSpace( double* array, int size, double xmin, double xmax )
{
    if ( ( xmin <= 0.0 ) || ( xmax <= 0.0 ) || ( size <= 0 ) )
        return;

    const int imax = size - 1;

    array[0] = xmin;
    array[imax] = xmax;

    const double lxmin = log( xmin );
    const double lxmax = log( xmax );
    const double lstep = ( lxmax - lxmin ) / double( imax );

    for ( int i = 1; i < imax; i++ )
        array[i] = std::exp( lxmin + double( i ) * lstep );
}

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
{
    setAutoReplot( false );

    setTitle( "Frequency Response of a Second-Order System" );

    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setBorderRadius( 10 );

    setCanvas( canvas );
    setCanvasBackground( QColor( "MidnightBlue" ) );

    // legend
    QwtLegend* legend = new QwtLegend;
    insertLegend( legend, QwtPlot::BottomLegend );

    // grid
    QwtPlotGrid* grid = new QwtPlotGrid;
    grid->enableXMin( true );
    grid->setMajorPen( Qt::white, 0, Qt::DotLine );
    grid->setMinorPen( Qt::gray, 0, Qt::DotLine );
    grid->attach( this );

    // axes
    setAxisVisible( QwtAxis::YRight );
    setAxisTitle( QwtAxis::XBottom, "Normalized Frequency" );
    setAxisTitle( QwtAxis::YLeft, "Amplitude [dB]" );
    setAxisTitle( QwtAxis::YRight, "Phase [deg]" );

    setAxisMaxMajor( QwtAxis::XBottom, 6 );
    setAxisMaxMinor( QwtAxis::XBottom, 9 );
    setAxisScaleEngine( QwtAxis::XBottom, new QwtLogScaleEngine );

    // curves
    m_curve1 = new QwtPlotCurve( "Amplitude" );
    m_curve1->setRenderHint( QwtPlotItem::RenderAntialiased );
    m_curve1->setPen( Qt::yellow );
    m_curve1->setLegendAttribute( QwtPlotCurve::LegendShowLine );
    m_curve1->setYAxis( QwtAxis::YLeft );
    m_curve1->attach( this );

    m_curve2 = new QwtPlotCurve( "Phase" );
    m_curve2->setRenderHint( QwtPlotItem::RenderAntialiased );
    m_curve2->setPen( Qt::cyan );
    m_curve2->setLegendAttribute( QwtPlotCurve::LegendShowLine );
    m_curve2->setYAxis( QwtAxis::YRight );
    m_curve2->attach( this );

    // marker
    m_marker1 = new QwtPlotMarker();
    m_marker1->setValue( 0.0, 0.0 );
    m_marker1->setLineStyle( QwtPlotMarker::VLine );
    m_marker1->setLabelAlignment( Qt::AlignRight | Qt::AlignBottom );
    m_marker1->setLinePen( Qt::green, 0, Qt::DashDotLine );
    m_marker1->attach( this );

    m_marker2 = new QwtPlotMarker();
    m_marker2->setLineStyle( QwtPlotMarker::HLine );
    m_marker2->setLabelAlignment( Qt::AlignRight | Qt::AlignBottom );
    m_marker2->setLinePen( QColor( 200, 150, 0 ), 0, Qt::DashDotLine );
    m_marker2->setSymbol( new QwtSymbol( QwtSymbol::Diamond,
        QColor( Qt::yellow ), QColor( Qt::green ), QSize( 8, 8 ) ) );
    m_marker2->attach( this );

    setDamp( 0.0 );

    setAutoReplot( true );
}

void Plot::showData( const double* frequency, const double* amplitude,
    const double* phase, int count )
{
    m_curve1->setSamples( frequency, amplitude, count );
    m_curve2->setSamples( frequency, phase, count );
}

void Plot::showPeak( double freq, double amplitude )
{
    QString label( "Peak: " );
    label += QString::number( amplitude, 'g', 3 );
    label += " dB";

    QwtText text( label );
    text.setFont( QFont( "Helvetica", 10, QFont::Bold ) );
    text.setColor( QColor( 200, 150, 0 ) );

    m_marker2->setValue( freq, amplitude );
    m_marker2->setLabel( text );
}

void Plot::show3dB( double freq )
{
    QString label( "-3 dB at f = " );
    label += QString::number( freq, 'g', 3 );

    QwtText text( label );
    text.setFont( QFont( "Helvetica", 10, QFont::Bold ) );
    text.setColor( Qt::green );

    m_marker1->setValue( freq, 0.0 );
    m_marker1->setLabel( text );
}

//
// re-calculate frequency response
//
void Plot::setDamp( double damping )
{
    const bool doReplot = autoReplot();
    setAutoReplot( false );

    const int ArraySize = 200;

    double frequency[ArraySize];
    double amplitude[ArraySize];
    double phase[ArraySize];

    // build frequency vector with logarithmic division
    logSpace( frequency, ArraySize, 0.01, 100 );

    int i3 = 1;
    double fmax = 1;
    double amax = -1000.0;

    for ( int i = 0; i < ArraySize; i++ )
    {
        double f = frequency[i];
        const ComplexNumber g =
            ComplexNumber( 1.0 ) / ComplexNumber( 1.0 - f * f, 2.0 * damping * f );

        amplitude[i] = 20.0 * log10( std::sqrt( g.real() * g.real() + g.imag() * g.imag() ) );
        phase[i] = std::atan2( g.imag(), g.real() ) * ( 180.0 / M_PI );

        if ( ( i3 <= 1 ) && ( amplitude[i] < -3.0 ) )
            i3 = i;
        if ( amplitude[i] > amax )
        {
            amax = amplitude[i];
            fmax = frequency[i];
        }

    }

    double f3 = frequency[i3] - ( frequency[i3] - frequency[i3 - 1] )
        / ( amplitude[i3] - amplitude[i3 - 1] ) * ( amplitude[i3] + 3 );

    showPeak( fmax, amax );
    show3dB( f3 );
    showData( frequency, amplitude, phase, ArraySize );

    setAutoReplot( doReplot );

    replot();
}

#include "moc_Plot.cpp"
