/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "BarChart.h"

#include <QwtPlotRenderer>
#include <QwtPlotMultiBarChart>
#include <QwtColumnSymbol>
#include <QwtPlotLayout>
#include <QwtLegend>
#include <QwtScaleDraw>
#include <QwtText>
#include <QwtMath>

BarChart::BarChart( QWidget* parent )
    : QwtPlot( parent )
{
    setAutoFillBackground( true );

    setPalette( Qt::white );
    canvas()->setPalette( QColor( "LemonChiffon" ) );

    setTitle( "Bar Chart" );

    setAxisTitle( QwtAxis::YLeft, "Whatever" );
    setAxisTitle( QwtAxis::XBottom, "Whatever" );

    m_barChartItem = new QwtPlotMultiBarChart( "Bar Chart " );
    m_barChartItem->setLayoutPolicy( QwtPlotMultiBarChart::AutoAdjustSamples );
    m_barChartItem->setSpacing( 20 );
    m_barChartItem->setMargin( 3 );

    m_barChartItem->attach( this );

    insertLegend( new QwtLegend() );

    populate();
    setOrientation( 0 );

    setAutoReplot( true );
}

void BarChart::populate()
{
    static const char* colors[] = { "DarkOrchid", "SteelBlue", "Gold" };

    const int numSamples = 5;
    const int numBars = sizeof( colors ) / sizeof( colors[0] );

    QList< QwtText > titles;
    for ( int i = 0; i < numBars; i++ )
    {
        QString title("Bar %1");
        titles += title.arg( i );
    }
    m_barChartItem->setBarTitles( titles );
    m_barChartItem->setLegendIconSize( QSize( 10, 14 ) );

    for ( int i = 0; i < numBars; i++ )
    {
        QwtColumnSymbol* symbol = new QwtColumnSymbol( QwtColumnSymbol::Box );
        symbol->setLineWidth( 2 );
        symbol->setFrameStyle( QwtColumnSymbol::Raised );
        symbol->setPalette( QPalette( colors[i] ) );

        m_barChartItem->setSymbol( i, symbol );
    }

    QVector< QVector< double > > series;
    for ( int i = 0; i < numSamples; i++ )
    {
        QVector< double > values;
        for ( int j = 0; j < numBars; j++ )
            values += ( 2 + qwtRand() % 8 );

        series += values;
    }

    m_barChartItem->setSamples( series );
}

void BarChart::setMode( int mode )
{
    if ( mode == 0 )
    {
        m_barChartItem->setStyle( QwtPlotMultiBarChart::Grouped );
    }
    else
    {
        m_barChartItem->setStyle( QwtPlotMultiBarChart::Stacked );
    }
}

void BarChart::setOrientation( int orientation )
{
    int axis1, axis2;

    if ( orientation == 0 )
    {
        axis1 = QwtAxis::XBottom;
        axis2 = QwtAxis::YLeft;

        m_barChartItem->setOrientation( Qt::Vertical );
    }
    else
    {
        axis1 = QwtAxis::YLeft;
        axis2 = QwtAxis::XBottom;

        m_barChartItem->setOrientation( Qt::Horizontal );
    }

    setAxisScale( axis1, 0, m_barChartItem->dataSize() - 1, 1.0 );
    setAxisAutoScale( axis2 );

    QwtScaleDraw* scaleDraw1 = axisScaleDraw( axis1 );
    scaleDraw1->enableComponent( QwtScaleDraw::Backbone, false );
    scaleDraw1->enableComponent( QwtScaleDraw::Ticks, false );

    QwtScaleDraw* scaleDraw2 = axisScaleDraw( axis2 );
    scaleDraw2->enableComponent( QwtScaleDraw::Backbone, true );
    scaleDraw2->enableComponent( QwtScaleDraw::Ticks, true );

    plotLayout()->setAlignCanvasToScale( axis1, true );
    plotLayout()->setAlignCanvasToScale( axis2, false );

    plotLayout()->setCanvasMargin( 0 );
    updateCanvasMargins();

    replot();
}

void BarChart::exportChart()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "barchart.pdf" );
}

#include "moc_BarChart.cpp"
