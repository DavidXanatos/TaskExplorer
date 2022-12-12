/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "Friedberg2007.h"

#include <QwtPlotZoomer>
#include <QwtPlotPanner>
#include <QwtPlotGrid>
#include <QwtPlotCurve>
#include <QwtPlotCanvas>
#include <QwtPlotIntervalCurve>
#include <QwtLegend>
#include <QwtIntervalSymbol>
#include <QwtSymbol>
#include <QwtText>
#include <QwtScaleDraw>
#include <QwtPlotRenderer>

#include <QLocale>

namespace
{
    class Grid : public QwtPlotGrid
    {
      public:
        Grid()
        {
            enableXMin( true );
            setMajorPen( Qt::white, 0, Qt::DotLine );
            setMinorPen( Qt::gray, 0, Qt::DotLine );
        }

        virtual void updateScaleDiv( const QwtScaleDiv& xScaleDiv,
            const QwtScaleDiv& yScaleDiv ) QWT_OVERRIDE
        {
            QwtScaleDiv scaleDiv( xScaleDiv.lowerBound(),
                xScaleDiv.upperBound() );

            scaleDiv.setTicks( QwtScaleDiv::MinorTick,
                xScaleDiv.ticks( QwtScaleDiv::MinorTick ) );

            scaleDiv.setTicks( QwtScaleDiv::MajorTick,
                xScaleDiv.ticks( QwtScaleDiv::MediumTick ) );

            QwtPlotGrid::updateScaleDiv( scaleDiv, yScaleDiv );
        }
    };

    class YearScaleDraw : public QwtScaleDraw
    {
      public:
        YearScaleDraw()
        {
            setTickLength( QwtScaleDiv::MajorTick, 0 );
            setTickLength( QwtScaleDiv::MinorTick, 0 );
            setTickLength( QwtScaleDiv::MediumTick, 6 );

            setLabelRotation( -60.0 );
            setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );

            setSpacing( 15 );
        }

        virtual QwtText label( double value ) const QWT_OVERRIDE
        {
            const int month = int( value / 30 ) + 1;
            return QLocale::system().monthName( month, QLocale::LongFormat );
        }
    };
}

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
{
    setObjectName( "FriedbergPlot" );
    setTitle( "Temperature of Friedberg/Germany" );

    setAxisTitle( QwtAxis::XBottom, "2007" );
    setAxisScaleDiv( QwtAxis::XBottom, yearScaleDiv() );
    setAxisScaleDraw( QwtAxis::XBottom, new YearScaleDraw() );

    setAxisTitle( QwtAxis::YLeft,
        QString( "Temperature [%1C]" ).arg( QChar( 0x00B0 ) ) );

    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setPalette( Qt::darkGray );
    canvas->setBorderRadius( 10 );

    setCanvas( canvas );

    // grid
    QwtPlotGrid* grid = new Grid;
    grid->attach( this );

    insertLegend( new QwtLegend(), QwtPlot::RightLegend );

    const int numDays = 365;
    QVector< QPointF > averageData( numDays );
    QVector< QwtIntervalSample > rangeData( numDays );

    for ( int i = 0; i < numDays; i++ )
    {
        const Temperature& t = friedberg2007[i];
        averageData[i] = QPointF( double( i ), t.averageValue );
        rangeData[i] = QwtIntervalSample( double( i ),
            QwtInterval( t.minValue, t.maxValue ) );
    }

    insertCurve( "Average", averageData, Qt::black );
    insertErrorBars( "Range", rangeData, Qt::blue );

    /*
        LeftButton for the zooming
        MidButton for the panning
        RightButton: zoom out by 1
        Ctrl+RighButton: zoom out to full size
     */

    QwtPlotZoomer* zoomer = new QwtPlotZoomer( canvas );
    zoomer->setRubberBandPen( QColor( Qt::black ) );
    zoomer->setTrackerPen( QColor( Qt::black ) );
    zoomer->setMousePattern( QwtEventPattern::MouseSelect2,
        Qt::RightButton, Qt::ControlModifier );
    zoomer->setMousePattern( QwtEventPattern::MouseSelect3,
        Qt::RightButton );

    QwtPlotPanner* panner = new QwtPlotPanner( canvas );
    panner->setMouseButton( Qt::MiddleButton );
}

QwtScaleDiv Plot::yearScaleDiv() const
{
    const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    QList< double > mediumTicks;
    mediumTicks += 0.0;
    for ( uint i = 0; i < sizeof( days ) / sizeof( days[0] ); i++ )
        mediumTicks += mediumTicks.last() + days[i];

    QList< double > minorTicks;
    for ( int i = 1; i <= 365; i += 7 )
        minorTicks += i;

    QList< double > majorTicks;
    for ( int i = 0; i < 12; i++ )
        majorTicks += i * 30 + 15;

    QwtScaleDiv scaleDiv( mediumTicks.first(), mediumTicks.last() + 1,
        minorTicks, mediumTicks, majorTicks );
    return scaleDiv;
}

void Plot::insertCurve( const QString& title,
    const QVector< QPointF >& samples, const QColor& color )
{
    m_curve = new QwtPlotCurve( title );
    m_curve->setRenderHint( QwtPlotItem::RenderAntialiased );
    m_curve->setStyle( QwtPlotCurve::NoCurve );
    m_curve->setLegendAttribute( QwtPlotCurve::LegendShowSymbol );

    QwtSymbol* symbol = new QwtSymbol( QwtSymbol::XCross );
    symbol->setSize( 4 );
    symbol->setPen( color );
    m_curve->setSymbol( symbol );

    m_curve->setSamples( samples );
    m_curve->attach( this );
}

void Plot::insertErrorBars(
    const QString& title,
    const QVector< QwtIntervalSample >& samples,
    const QColor& color )
{
    m_intervalCurve = new QwtPlotIntervalCurve( title );
    m_intervalCurve->setRenderHint( QwtPlotItem::RenderAntialiased );
    m_intervalCurve->setPen( Qt::white );

    QColor bg( color );
    bg.setAlpha( 150 );
    m_intervalCurve->setBrush( QBrush( bg ) );
    m_intervalCurve->setStyle( QwtPlotIntervalCurve::Tube );

    m_intervalCurve->setSamples( samples );
    m_intervalCurve->attach( this );
}

void Plot::setMode( int style )
{
    if ( style == Tube )
    {
        m_intervalCurve->setStyle( QwtPlotIntervalCurve::Tube );
        m_intervalCurve->setSymbol( NULL );
        m_intervalCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
    }
    else
    {
        m_intervalCurve->setStyle( QwtPlotIntervalCurve::NoCurve );

        QColor c( m_intervalCurve->brush().color().rgb() ); // skip alpha

        QwtIntervalSymbol* errorBar =
            new QwtIntervalSymbol( QwtIntervalSymbol::Bar );
        errorBar->setWidth( 8 ); // should be something even
        errorBar->setPen( c );

        m_intervalCurve->setSymbol( errorBar );
        m_intervalCurve->setRenderHint( QwtPlotItem::RenderAntialiased, false );
    }

    replot();
}

void Plot::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "friedberg.pdf" );
}

#include "moc_Plot.cpp"
