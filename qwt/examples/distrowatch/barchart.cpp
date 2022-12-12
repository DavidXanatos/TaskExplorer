/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "BarChart.h"

#include <QwtPlotRenderer>
#include <QwtPlotCanvas>
#include <QwtPlotBarChart>
#include <QwtColumnSymbol>
#include <QwtPlotLayout>
#include <QwtLegend>
#include <QwtScaleDraw>
#include <QwtText>

#include <QApplication>
#include <QPainter>

namespace
{
    class ScaleDraw : public QwtScaleDraw
    {
      public:
        ScaleDraw( Qt::Orientation orientation, const QStringList& labels )
            : m_labels( labels )
        {
            setTickLength( QwtScaleDiv::MinorTick, 0 );
            setTickLength( QwtScaleDiv::MediumTick, 0 );
            setTickLength( QwtScaleDiv::MajorTick, 2 );

            enableComponent( QwtScaleDraw::Backbone, false );

            if ( orientation == Qt::Vertical )
            {
                setLabelRotation( -60.0 );
            }
            else
            {
                setLabelRotation( -20.0 );
            }

            setLabelAlignment( Qt::AlignLeft | Qt::AlignVCenter );
        }

        virtual QwtText label( double value ) const QWT_OVERRIDE
        {
            QwtText lbl;

            const int index = qRound( value );
            if ( index >= 0 && index < m_labels.size() )
            {
                lbl = m_labels[ index ];
            }

            return lbl;
        }

      private:
        const QStringList m_labels;
    };

    class ChartItem : public QwtPlotBarChart
    {
      public:
        ChartItem()
            : QwtPlotBarChart( "Page Hits" )
        {
            setLegendMode( QwtPlotBarChart::LegendBarTitles );
            setLegendIconSize( QSize( 10, 14 ) );
            setLayoutPolicy( AutoAdjustSamples );
            setLayoutHint( 4.0 ); // minimum width for a single bar

            setSpacing( 10 ); // spacing between bars
        }

        void addDistro( const QString& distro, const QColor& color )
        {
            m_colors += color;
            m_distros += distro;
            itemChanged();
        }

        virtual QwtColumnSymbol* specialSymbol(
            int index, const QPointF& ) const QWT_OVERRIDE
        {
            // we want to have individual colors for each bar

            QwtColumnSymbol* symbol = new QwtColumnSymbol( QwtColumnSymbol::Box );
            symbol->setLineWidth( 2 );
            symbol->setFrameStyle( QwtColumnSymbol::Raised );

            QColor c( Qt::white );
            if ( index >= 0 && index < m_colors.size() )
                c = m_colors[ index ];

            symbol->setPalette( c );

            return symbol;
        }

        virtual QwtText barTitle( int sampleIndex ) const QWT_OVERRIDE
        {
            QwtText title;
            if ( sampleIndex >= 0 && sampleIndex < m_distros.size() )
                title = m_distros[ sampleIndex ];

            return title;
        }

      private:
        QList< QColor > m_colors;
        QList< QString > m_distros;
    };

}

BarChart::BarChart( QWidget* parent )
    : QwtPlot( parent )
{
    const struct
    {
        const char* distro;
        const int hits;
        QColor color;

    } pageHits[] =
    {
        { "Arch", 1114, QColor( "DodgerBlue" ) },
        { "Debian", 1373, QColor( "#d70751" ) },
        { "Fedora", 1638, QColor( "SteelBlue" ) },
        { "Mageia", 1395, QColor( "Indigo" ) },
        { "Mint", 3874, QColor( 183, 255, 183 ) },
        { "openSuSE", 1532, QColor( 115, 186, 37 ) },
        { "Puppy", 1059, QColor( "LightSkyBlue" ) },
        { "Ubuntu", 2391, QColor( "FireBrick" ) }
    };

    setAutoFillBackground( true );
    setPalette( QColor( "Linen" ) );

    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setLineWidth( 2 );
    canvas->setFrameStyle( QFrame::Box | QFrame::Sunken );
    canvas->setBorderRadius( 10 );

    QPalette canvasPalette( QColor( "Plum" ) );
    canvasPalette.setColor( QPalette::WindowText, QColor( "Indigo" ) );
    canvas->setPalette( canvasPalette );

    setCanvas( canvas );

    setTitle( "DistroWatch Page Hit Ranking, April 2012" );

    ChartItem* chartItem = new ChartItem();

    QVector< double > samples;

    for ( uint i = 0; i < sizeof( pageHits ) / sizeof( pageHits[ 0 ] ); i++ )
    {
        m_distros += pageHits[ i ].distro;
        samples += pageHits[ i ].hits;

        chartItem->addDistro( pageHits[ i ].distro, pageHits[ i ].color );
    }

    chartItem->setSamples( samples );
    chartItem->attach( this );

    m_chartItem = chartItem;

    insertLegend( new QwtLegend() );

    setOrientation( 0 );
    setAutoReplot( false );
}

void BarChart::setOrientation( int o )
{
    const Qt::Orientation orientation =
        ( o == 0 ) ? Qt::Vertical : Qt::Horizontal;

    int axis1 = QwtAxis::XBottom;
    int axis2 = QwtAxis::YLeft;

    if ( orientation == Qt::Horizontal )
        qSwap( axis1, axis2 );

    m_chartItem->setOrientation( orientation );

    setAxisTitle( axis1, "Distros" );
    setAxisMaxMinor( axis1, 3 );
    setAxisScaleDraw( axis1, new ScaleDraw( orientation, m_distros ) );

    setAxisTitle( axis2, "Hits per day ( HPD )" );
    setAxisMaxMinor( axis2, 3 );

    QwtScaleDraw* scaleDraw = new QwtScaleDraw();
    scaleDraw->setTickLength( QwtScaleDiv::MediumTick, 4 );
    setAxisScaleDraw( axis2, scaleDraw );

    plotLayout()->setCanvasMargin( 0 );
    replot();
}

void BarChart::exportChart()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( this, "distrowatch.pdf" );
}

void BarChart::doScreenShot()
{
    exportPNG( 800, 600 );
}

void BarChart::exportPNG( int width, int height )
{
    const QString fileBase = QString("distrowatch-%2x%3" ).arg( width ).arg( height);

    const int resolution = qRound( 85.0 * width / 800.0 );

    const double mmToInch = 1.0 / 25.4;
    const int dotsPerMeter = qRound( resolution * mmToInch * 1000.0 );

    QImage image( width, height, QImage::Format_ARGB32 );
    image.setDotsPerMeterX( dotsPerMeter );
    image.setDotsPerMeterY( dotsPerMeter );

    image.fill( Qt::transparent );

    QPainter painter( &image );
    render( &painter, QRectF( 0, 0, width, height ) );
    painter.end();

    image.save( fileBase + ".png" );
}

void BarChart::render( QPainter* painter, const QRectF& targetRect )
{
    const int r = 20;
    const QRectF plotRect = targetRect.adjusted( 0.5 * r, 0.5 * r, -0.5 * r, -0.5 * r );

    QwtPlotRenderer renderer;

    if ( qApp->styleSheet().isEmpty() )
    {
        renderer.setDiscardFlag( QwtPlotRenderer::DiscardBackground, true );

        painter->save();
        painter->setRenderHint( QPainter::Antialiasing, true );
        painter->setPen( QPen( Qt::darkGray, 1 ) );
        painter->setBrush( QColor( "WhiteSmoke" ) );
        painter->drawRoundedRect( targetRect, r, r );
        painter->restore();
    }

    renderer.render( this, painter, plotRect );
}

#include "moc_BarChart.cpp"
