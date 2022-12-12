/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "TVPlot.h"

#include <QwtPlotLayout>
#include <QwtPlotCanvas>
#include <QwtPlotRenderer>
#include <QwtLegend>
#include <QwtLegendLabel>
#include <QwtPlotGrid>
#include <QwtPlotHistogram>
#include <QwtColumnSymbol>
#include <QwtSeriesData>

#include <QPen>
#include <cstdlib>

namespace
{
    class Histogram : public QwtPlotHistogram
    {
      public:
        Histogram( const QString& title, const QColor& symbolColor )
            : QwtPlotHistogram( title )
        {
            setStyle( QwtPlotHistogram::Columns );
            setColor( symbolColor );
        }

        void setColor( const QColor& color )
        {
            QColor c = color;
            c.setAlpha( 180 );
            setBrush( QBrush( c ) );
        }

        void setValues( uint numValues, const double* values )
        {
            QVector< QwtIntervalSample > samples( numValues );
            for ( uint i = 0; i < numValues; i++ )
            {
                QwtInterval interval( double( i ), i + 1.0 );
                interval.setBorderFlags( QwtInterval::ExcludeMaximum );

                samples[i] = QwtIntervalSample( values[i], interval );
            }

            setData( new QwtIntervalSeriesData( samples ) );
        }
    };
}

TVPlot::TVPlot( QWidget* parent )
    : QwtPlot( parent )
{
    setTitle( "Watching TV during a weekend" );

    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setPalette( Qt::gray );
    canvas->setBorderRadius( 10 );
    setCanvas( canvas );

    plotLayout()->setAlignCanvasToScales( true );

    setAxisTitle( QwtAxis::YLeft, "Number of People" );
    setAxisTitle( QwtAxis::XBottom, "Number of Hours" );

    QwtLegend* legend = new QwtLegend;
    legend->setDefaultItemMode( QwtLegendData::Checkable );
    insertLegend( legend, QwtPlot::RightLegend );

    populate();

    connect( legend, SIGNAL(checked(const QVariant&,bool,int)),
        SLOT(showItem(const QVariant&,bool)) );

    replot(); // creating the legend items

    QwtPlotItemList items = itemList( QwtPlotItem::Rtti_PlotHistogram );
    for ( int i = 0; i < items.size(); i++ )
    {
        if ( i == 0 )
        {
            const QVariant itemInfo = itemToInfo( items[i] );

            QwtLegendLabel* legendLabel =
                qobject_cast< QwtLegendLabel* >( legend->legendWidget( itemInfo ) );
            if ( legendLabel )
                legendLabel->setChecked( true );

            items[i]->setVisible( true );
        }
        else
        {
            items[i]->setVisible( false );
        }
    }

    setAutoReplot( true );
}

void TVPlot::populate()
{
    QwtPlotGrid* grid = new QwtPlotGrid;
    grid->enableX( false );
    grid->enableY( true );
    grid->enableXMin( false );
    grid->enableYMin( false );
    grid->setMajorPen( Qt::black, 0, Qt::DotLine );
    grid->attach( this );

    const double juneValues[] = { 7, 19, 24, 32, 10, 5, 3 };
    const double novemberValues[] = { 4, 15, 22, 34, 13, 8, 4 };

    Histogram* histogramJune = new Histogram( "Summer", Qt::red );
    histogramJune->setValues(
        sizeof( juneValues ) / sizeof( double ), juneValues );
    histogramJune->attach( this );

    Histogram* histogramNovember = new Histogram( "Winter", Qt::blue );
    histogramNovember->setValues(
        sizeof( novemberValues ) / sizeof( double ), novemberValues );
    histogramNovember->attach( this );
}

void TVPlot::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "tvplot.pdf" );
}

void TVPlot::setMode( int mode )
{
    QwtPlotItemList items = itemList( QwtPlotItem::Rtti_PlotHistogram );

    for ( int i = 0; i < items.size(); i++ )
    {
        QwtPlotHistogram* histogram = static_cast< QwtPlotHistogram* >( items[i] );
        if ( mode < 3 )
        {
            histogram->setStyle( static_cast< QwtPlotHistogram::HistogramStyle >( mode ) );
            histogram->setSymbol( NULL );

            QPen pen( Qt::black, 0 );
            if ( mode == QwtPlotHistogram::Lines )
                pen.setBrush( histogram->brush() );

            histogram->setPen( pen );
        }
        else
        {
            histogram->setStyle( QwtPlotHistogram::Columns );

            QwtColumnSymbol* symbol = new QwtColumnSymbol( QwtColumnSymbol::Box );
            symbol->setFrameStyle( QwtColumnSymbol::Raised );
            symbol->setLineWidth( 2 );
            symbol->setPalette( QPalette( histogram->brush().color() ) );

            histogram->setSymbol( symbol );
        }
    }
}

void TVPlot::showItem( const QVariant& itemInfo, bool on )
{
    if ( QwtPlotItem* plotItem = infoToItem( itemInfo ) )
        plotItem->setVisible( on );
}

#include "moc_TVPlot.cpp"
