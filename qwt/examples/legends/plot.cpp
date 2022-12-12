/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "Settings.h"

#include <QwtPlotCurve>
#include <QwtPlotLegendItem>
#include <QwtLegend>
#include <QwtPlotCanvas>
#include <QwtPlotGrid>
#include <QwtPlotLayout>
#include <QwtMath>

#include <QPen>

namespace
{
    class LegendItem : public QwtPlotLegendItem
    {
      public:
        LegendItem()
        {
            setRenderHint( QwtPlotItem::RenderAntialiased );

            const QColor c1( Qt::white );

            setTextPen( c1 );
            setBorderPen( c1 );

            QColor c2( Qt::gray );
            c2.setAlpha( 200 );

            setBackgroundBrush( c2 );
        }
    };

    class Curve : public QwtPlotCurve
    {
      public:
        Curve( int index ):
            m_index( index )
        {
            setRenderHint( QwtPlotItem::RenderAntialiased );
            initData();
        }

        void setCurveTitle( const QString& title )
        {
            QString txt("%1 %2");
            setTitle( QString( "%1 %2" ).arg( title ).arg( m_index ) );
        }

        void initData()
        {
            QVector< QPointF > points;

            double y = qwtRand() % 1000;

            for ( double x = 0.0; x <= 1000.0; x += 100.0 )
            {
                double off = qwtRand() % 200 - 100;
                if ( y + off > 980.0 || y + off < 20.0 )
                    off = -off;

                y += off;

                points += QPointF( x, y );
            }

            setSamples( points );
        }

      private:
        const int m_index;
    };
}

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
    , m_externalLegend( NULL )
    , m_legendItem( NULL )
    , m_isDirty( false )
{
    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setFocusIndicator( QwtPlotCanvas::CanvasFocusIndicator );
    canvas->setFocusPolicy( Qt::StrongFocus );
    canvas->setPalette( Qt::black );
    setCanvas( canvas );

    setAutoReplot( false );

    setTitle( "Legend Test" );
    setFooter( "Footer" );

    // grid
    QwtPlotGrid* grid = new QwtPlotGrid;
    grid->enableXMin( true );
    grid->setMajorPen( Qt::gray, 0, Qt::DotLine );
    grid->setMinorPen( Qt::darkGray, 0, Qt::DotLine );
    grid->attach( this );

    // axis
    setAxisScale( QwtAxis::YLeft, 0.0, 1000.0 );
    setAxisScale( QwtAxis::XBottom, 0.0, 1000.0 );
}

Plot::~Plot()
{
    delete m_externalLegend;
}

void Plot::insertCurve()
{
    static int counter = 1;

    const char* colors[] =
    {
        "LightSalmon",
        "SteelBlue",
        "Yellow",
        "Fuchsia",
        "PaleGreen",
        "PaleTurquoise",
        "Cornsilk",
        "HotPink",
        "Peru",
        "Maroon"
    };
    const int numColors = sizeof( colors ) / sizeof( colors[0] );

    QwtPlotCurve* curve = new Curve( counter++ );
    curve->setPen( QColor( colors[ counter % numColors ] ), 2 );
    curve->attach( this );
}

void Plot::applySettings( const Settings& settings )
{
    m_isDirty = false;
    setAutoReplot( true );

    if ( settings.legend.isEnabled )
    {
        if ( settings.legend.position > QwtPlot::TopLegend )
        {
            if ( legend() )
            {
                // remove legend controlled by the plot
                insertLegend( NULL );
            }

            if ( m_externalLegend == NULL )
            {
                m_externalLegend = new QwtLegend();
                m_externalLegend->setWindowTitle("Plot Legend");

                connect(
                    this,
                    SIGNAL(legendDataChanged(const QVariant&,const QList<QwtLegendData>&)),
                    m_externalLegend,
                    SLOT(updateLegend(const QVariant&,const QList<QwtLegendData>&)) );

                m_externalLegend->show();

                // populate the new legend
                updateLegend();
            }
        }
        else
        {
            delete m_externalLegend;
            m_externalLegend = NULL;

            if ( legend() == NULL ||
                plotLayout()->legendPosition() != settings.legend.position )
            {
                insertLegend( new QwtLegend(),
                    QwtPlot::LegendPosition( settings.legend.position ) );
            }
        }
    }
    else
    {
        insertLegend( NULL );

        delete m_externalLegend;
        m_externalLegend = NULL;
    }

    if ( settings.legendItem.isEnabled )
    {
        if ( m_legendItem == NULL )
        {
            m_legendItem = new LegendItem();
            m_legendItem->attach( this );
        }

        m_legendItem->setMaxColumns( settings.legendItem.numColumns );
        m_legendItem->setAlignmentInCanvas( Qt::Alignment( settings.legendItem.alignment ) );
        m_legendItem->setBackgroundMode(
            QwtPlotLegendItem::BackgroundMode( settings.legendItem.backgroundMode ) );
        if ( settings.legendItem.backgroundMode ==
            QwtPlotLegendItem::ItemBackground )
        {
            m_legendItem->setBorderRadius( 4 );
            m_legendItem->setMargin( 0 );
            m_legendItem->setSpacing( 4 );
            m_legendItem->setItemMargin( 2 );
        }
        else
        {
            m_legendItem->setBorderRadius( 8 );
            m_legendItem->setMargin( 4 );
            m_legendItem->setSpacing( 2 );
            m_legendItem->setItemMargin( 0 );
        }

        QFont font = m_legendItem->font();
        font.setPointSize( settings.legendItem.size );
        m_legendItem->setFont( font );
    }
    else
    {
        delete m_legendItem;
        m_legendItem = NULL;
    }

    QwtPlotItemList curveList = itemList( QwtPlotItem::Rtti_PlotCurve );
    if ( curveList.size() != settings.curve.numCurves )
    {
        while ( curveList.size() > settings.curve.numCurves )
        {
            QwtPlotItem* curve = curveList.takeFirst();
            delete curve;
        }

        for ( int i = curveList.size(); i < settings.curve.numCurves; i++ )
            insertCurve();
    }

    curveList = itemList( QwtPlotItem::Rtti_PlotCurve );
    for ( int i = 0; i < curveList.count(); i++ )
    {
        Curve* curve = static_cast< Curve* >( curveList[i] );
        curve->setCurveTitle( settings.curve.title );

        int sz = 0.5 * settings.legendItem.size;
        curve->setLegendIconSize( QSize( sz, sz ) );
    }

    setAutoReplot( false );
    if ( m_isDirty )
    {
        m_isDirty = false;
        replot();
    }
}

void Plot::replot()
{
    if ( autoReplot() )
    {
        m_isDirty = true;
        return;
    }

    QwtPlot::replot();
}

#include "moc_Plot.cpp"
