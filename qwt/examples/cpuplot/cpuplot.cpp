/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "CpuPieMarker.h"
#include "CpuPlot.h"

#include <QwtPlotLayout>
#include <QwtPlotCurve>
#include <QwtScaleDraw>
#include <QwtScaleMap>
#include <QwtScaleWidget>
#include <QwtLegend>
#include <QwtLegendLabel>
#include <QwtPlotCanvas>

#include <QPainter>

namespace
{
    class TimeScaleDraw : public QwtScaleDraw
    {
      public:
        TimeScaleDraw( const QTime& base )
            : baseTime( base )
        {
        }
        virtual QwtText label( double v ) const QWT_OVERRIDE
        {
            QTime upTime = baseTime.addSecs( static_cast< int >( v ) );
            return upTime.toString();
        }
      private:
        QTime baseTime;
    };

    class Background : public QwtPlotItem
    {
      public:
        Background()
        {
            setZ( 0.0 );
        }

        virtual int rtti() const QWT_OVERRIDE
        {
            return QwtPlotItem::Rtti_PlotUserItem;
        }

        virtual void draw( QPainter* painter,
            const QwtScaleMap&, const QwtScaleMap& yMap,
            const QRectF& canvasRect ) const QWT_OVERRIDE
        {
            QColor c( Qt::white );
            QRectF r = canvasRect;

            for ( int i = 100; i > 0; i -= 10 )
            {
                r.setBottom( yMap.transform( i - 10 ) );
                r.setTop( yMap.transform( i ) );
                painter->fillRect( r, c );

                c = c.darker( 110 );
            }
        }
    };

    class CpuCurve : public QwtPlotCurve
    {
      public:
        CpuCurve( const QString& title )
            : QwtPlotCurve( title )
        {
            setRenderHint( QwtPlotItem::RenderAntialiased );
        }

        void setColor( const QColor& color )
        {
            QColor c = color;
            c.setAlpha( 150 );

            setPen( QPen( Qt::NoPen ) );
            setBrush( c );
        }
    };
}

CpuPlot::CpuPlot( QWidget* parent )
    : QwtPlot( parent )
    , m_dataCount( 0 )
{
    using namespace QwtAxis;

    setAutoReplot( false );

    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setBorderRadius( 10 );

    setCanvas( canvas );

    plotLayout()->setAlignCanvasToScales( true );

    QwtLegend* legend = new QwtLegend;
    legend->setDefaultItemMode( QwtLegendData::Checkable );
    insertLegend( legend, QwtPlot::RightLegend );

    setAxisTitle( XBottom, " System Uptime [h:m:s]" );
    setAxisScaleDraw( XBottom, new TimeScaleDraw( m_cpuStat.upTime() ) );
    setAxisScale( XBottom, 0, HISTORY );
    setAxisLabelRotation( XBottom, -50.0 );
    setAxisLabelAlignment( XBottom, Qt::AlignLeft | Qt::AlignBottom );

    /*
       In situations, when there is a label at the most right position of the
       scale, additional space is needed to display the overlapping part
       of the label would be taken by reducing the width of scale and canvas.
       To avoid this "jumping canvas" effect, we add a permanent margin.
       We don't need to do the same for the left border, because there
       is enough space for the overlapping label below the left scale.
     */

    QwtScaleWidget* scaleWidget = axisWidget( XBottom );
    const int fmh = QFontMetrics( scaleWidget->font() ).height();
    scaleWidget->setMinBorderDist( 0, fmh / 2 );

    setAxisTitle( YLeft, "Cpu Usage [%]" );
    setAxisScale( YLeft, 0, 100 );

    Background* bg = new Background();
    bg->attach( this );

    CpuPieMarker* pie = new CpuPieMarker();
    pie->attach( this );

    CpuCurve* curve;

    curve = new CpuCurve( "System" );
    curve->setColor( Qt::red );
    curve->attach( this );
    m_data[System].curve = curve;

    curve = new CpuCurve( "User" );
    curve->setColor( Qt::blue );
    curve->setZ( curve->z() - 1 );
    curve->attach( this );
    m_data[User].curve = curve;

    curve = new CpuCurve( "Total" );
    curve->setColor( Qt::black );
    curve->setZ( curve->z() - 2 );
    curve->attach( this );
    m_data[Total].curve = curve;

    curve = new CpuCurve( "Idle" );
    curve->setColor( Qt::darkCyan );
    curve->setZ( curve->z() - 3 );
    curve->attach( this );
    m_data[Idle].curve = curve;

    showCurve( m_data[System].curve, true );
    showCurve( m_data[User].curve, true );
    showCurve( m_data[Total].curve, false );
    showCurve( m_data[Idle].curve, false );

    for ( int i = 0; i < HISTORY; i++ )
        m_timeData[HISTORY - 1 - i] = i;

    ( void )startTimer( 1000 ); // 1 second

    connect( legend, SIGNAL(checked(const QVariant&,bool,int)),
        SLOT(legendChecked(const QVariant&,bool)) );
}

void CpuPlot::timerEvent( QTimerEvent* )
{
    for ( int i = m_dataCount; i > 0; i-- )
    {
        for ( int c = 0; c < NCpuData; c++ )
        {
            if ( i < HISTORY )
                m_data[c].data[i] = m_data[c].data[i - 1];
        }
    }

    m_cpuStat.statistic( m_data[User].data[0], m_data[System].data[0] );

    m_data[Total].data[0] = m_data[User].data[0] + m_data[System].data[0];
    m_data[Idle].data[0] = 100.0 - m_data[Total].data[0];

    if ( m_dataCount < HISTORY )
        m_dataCount++;

    for ( int j = 0; j < HISTORY; j++ )
        m_timeData[j]++;

    setAxisScale( QwtAxis::XBottom,
        m_timeData[HISTORY - 1], m_timeData[0] );

    for ( int c = 0; c < NCpuData; c++ )
    {
        m_data[c].curve->setRawSamples(
            m_timeData, m_data[c].data, m_dataCount );
    }

    replot();
}

void CpuPlot::legendChecked( const QVariant& itemInfo, bool on )
{
    QwtPlotItem* plotItem = infoToItem( itemInfo );
    if ( plotItem )
        showCurve( plotItem, on );
}

void CpuPlot::showCurve( QwtPlotItem* item, bool on )
{
    item->setVisible( on );

    QwtLegend* lgd = qobject_cast< QwtLegend* >( legend() );

    QList< QWidget* > legendWidgets =
        lgd->legendWidgets( itemToInfo( item ) );

    if ( legendWidgets.size() == 1 )
    {
        QwtLegendLabel* legendLabel =
            qobject_cast< QwtLegendLabel* >( legendWidgets[0] );

        if ( legendLabel )
            legendLabel->setChecked( on );
    }

    replot();
}

#include "moc_CpuPlot.cpp"
