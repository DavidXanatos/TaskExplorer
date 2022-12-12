/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QwtSeriesData>
#include <QwtSymbol>
#include <QwtLegend>
#include <QwtPolarGrid>
#include <QwtPolarCurve>
#include <QwtPolarMarker>
#include <QwtScaleEngine>

#include <QPen>

static const QwtInterval s_radialInterval( 0.0, 10.0 );
static const QwtInterval s_azimuthInterval( 0.0, 360.0 );

namespace
{
    class Data : public QwtSeriesData< QwtPointPolar >
    {
      public:
        Data( const QwtInterval& radialInterval,
                const QwtInterval& azimuthInterval, size_t size )
            : m_radialInterval( radialInterval )
            , m_azimuthInterval( azimuthInterval )
            , m_size( size )
        {
        }

        virtual size_t size() const QWT_OVERRIDE
        {
            return m_size;
        }

      protected:
        QwtInterval m_radialInterval;
        QwtInterval m_azimuthInterval;
        size_t m_size;
    };

    class SpiralData : public Data
    {
      public:
        SpiralData( const QwtInterval& radialInterval,
                const QwtInterval& azimuthInterval, size_t size )
            : Data( radialInterval, azimuthInterval, size )
        {
        }

        virtual QwtPointPolar sample( size_t i ) const QWT_OVERRIDE
        {
            const double stepA = 4 * m_azimuthInterval.width() / m_size;
            const double a = m_azimuthInterval.minValue() + i * stepA;

            const double stepR = m_radialInterval.width() / m_size;
            const double r = m_radialInterval.minValue() + i * stepR;

            return QwtPointPolar( a, r );
        }

        virtual QRectF boundingRect() const QWT_OVERRIDE
        {
            if ( cachedBoundingRect.width() < 0.0 )
                cachedBoundingRect = qwtBoundingRect( *this );

            return cachedBoundingRect;
        }
    };

    class RoseData : public Data
    {
      public:
        RoseData( const QwtInterval& radialInterval,
                const QwtInterval& azimuthInterval, size_t size )
            : Data( radialInterval, azimuthInterval, size )
        {
        }

        virtual QwtPointPolar sample( size_t i ) const QWT_OVERRIDE
        {
            const double stepA = m_azimuthInterval.width() / m_size;
            const double a = m_azimuthInterval.minValue() + i * stepA;

            const double d = a / 360.0 * M_PI;
            const double r = m_radialInterval.maxValue() * qAbs( qSin( 4 * d ) );

            return QwtPointPolar( a, r );
        }

        virtual QRectF boundingRect() const QWT_OVERRIDE
        {
            if ( cachedBoundingRect.width() < 0.0 )
                cachedBoundingRect = qwtBoundingRect( *this );

            return cachedBoundingRect;
        }
    };
}

Plot::Plot( QWidget* parent )
    : QwtPolarPlot( QwtText( "Polar Plot Demo" ), parent )
{
    setAutoReplot( false );
    setPlotBackground( Qt::darkBlue );

    // scales
    setScale( QwtPolar::Azimuth,
        s_azimuthInterval.minValue(), s_azimuthInterval.maxValue(),
        s_azimuthInterval.width() / 12 );

    setScaleMaxMinor( QwtPolar::Azimuth, 2 );
    setScale( QwtPolar::Radius,
        s_radialInterval.minValue(), s_radialInterval.maxValue() );

    // grids, axes

    m_grid = new QwtPolarGrid();
    m_grid->setPen( QPen( Qt::white ) );
    for ( int scaleId = 0; scaleId < QwtPolar::ScaleCount; scaleId++ )
    {
        m_grid->showGrid( scaleId );
        m_grid->showMinorGrid( scaleId );

        QPen minorPen( Qt::gray );
#if 0
        minorPen.setStyle( Qt::DotLine );
#endif
        m_grid->setMinorGridPen( scaleId, minorPen );
    }
    m_grid->setAxisPen( QwtPolar::AxisAzimuth, QPen( Qt::black ) );

    m_grid->showAxis( QwtPolar::AxisAzimuth, true );
    m_grid->showAxis( QwtPolar::AxisLeft, false );
    m_grid->showAxis( QwtPolar::AxisRight, true );
    m_grid->showAxis( QwtPolar::AxisTop, true );
    m_grid->showAxis( QwtPolar::AxisBottom, false );
    m_grid->showGrid( QwtPolar::Azimuth, true );
    m_grid->showGrid( QwtPolar::Radius, true );
    m_grid->attach( this );

    // curves

    for ( int curveId = 0; curveId < PlotSettings::NumCurves; curveId++ )
    {
        m_curve[curveId] = createCurve( curveId );
        m_curve[curveId]->attach( this );
    }

    // markers
    QwtPolarMarker* marker = new QwtPolarMarker();
    marker->setPosition( QwtPointPolar( 57.3, 4.72 ) );
    marker->setSymbol( new QwtSymbol( QwtSymbol::Ellipse,
        QBrush( Qt::white ), QPen( Qt::green ), QSize( 9, 9 ) ) );
    marker->setLabelAlignment( Qt::AlignHCenter | Qt::AlignTop );

    QwtText text( "Marker" );
    text.setColor( Qt::black );
    QColor bg( Qt::white );
    bg.setAlpha( 200 );
    text.setBackgroundBrush( QBrush( bg ) );

    marker->setLabel( text );
    marker->attach( this );

    QwtLegend* legend = new QwtLegend;
    insertLegend( legend,  QwtPolarPlot::BottomLegend );
}

PlotSettings Plot::settings() const
{
    PlotSettings s;
    for ( int scaleId = 0; scaleId < QwtPolar::ScaleCount; scaleId++ )
    {
        s.flags[PlotSettings::MajorGridBegin + scaleId] =
            m_grid->isGridVisible( scaleId );
        s.flags[PlotSettings::MinorGridBegin + scaleId] =
            m_grid->isMinorGridVisible( scaleId );
    }
    for ( int axisId = 0; axisId < QwtPolar::AxesCount; axisId++ )
    {
        s.flags[PlotSettings::AxisBegin + axisId] =
            m_grid->isAxisVisible( axisId );
    }

    s.flags[PlotSettings::AutoScaling] =
        m_grid->testGridAttribute( QwtPolarGrid::AutoScaling );

    s.flags[PlotSettings::Logarithmic] =
        scaleEngine( QwtPolar::Radius )->transformation();

    const QwtScaleDiv* sd = scaleDiv( QwtPolar::Radius );
    s.flags[PlotSettings::Inverted] = sd->lowerBound() > sd->upperBound();

    s.flags[PlotSettings::Antialiasing] =
        m_grid->testRenderHint( QwtPolarItem::RenderAntialiased );

    for ( int curveId = 0; curveId < PlotSettings::NumCurves; curveId++ )
    {
        s.flags[PlotSettings::CurveBegin + curveId] =
            m_curve[curveId]->isVisible();
    }

    return s;
}

void Plot::applySettings( const PlotSettings& s )
{
    for ( int scaleId = 0; scaleId < QwtPolar::ScaleCount; scaleId++ )
    {
        m_grid->showGrid( scaleId,
            s.flags[PlotSettings::MajorGridBegin + scaleId] );
        m_grid->showMinorGrid( scaleId,
            s.flags[PlotSettings::MinorGridBegin + scaleId] );
    }

    for ( int axisId = 0; axisId < QwtPolar::AxesCount; axisId++ )
    {
        m_grid->showAxis( axisId,
            s.flags[PlotSettings::AxisBegin + axisId] );
    }

    m_grid->setGridAttribute( QwtPolarGrid::AutoScaling,
        s.flags[PlotSettings::AutoScaling] );

    const QwtInterval interval =
        scaleDiv( QwtPolar::Radius )->interval().normalized();
    if ( s.flags[PlotSettings::Inverted] )
    {
        setScale( QwtPolar::Radius,
            interval.maxValue(), interval.minValue() );
    }
    else
    {
        setScale( QwtPolar::Radius,
            interval.minValue(), interval.maxValue() );
    }

    if ( s.flags[PlotSettings::Logarithmic] )
    {
        setScaleEngine( QwtPolar::Radius, new QwtLogScaleEngine() );
    }
    else
    {
        setScaleEngine( QwtPolar::Radius, new QwtLinearScaleEngine() );
    }

    m_grid->setRenderHint( QwtPolarItem::RenderAntialiased,
        s.flags[PlotSettings::Antialiasing] );

    for ( int curveId = 0; curveId < PlotSettings::NumCurves; curveId++ )
    {
        m_curve[curveId]->setRenderHint( QwtPolarItem::RenderAntialiased,
            s.flags[PlotSettings::Antialiasing] );
        m_curve[curveId]->setVisible(
            s.flags[PlotSettings::CurveBegin + curveId] );
    }

    replot();
}

QwtPolarCurve* Plot::createCurve( int id ) const
{
    const int numPoints = 200;

    QwtPolarCurve* curve = new QwtPolarCurve();
    curve->setStyle( QwtPolarCurve::Lines );
    //curve->setLegendAttribute( QwtPolarCurve::LegendShowLine, true );
    //curve->setLegendAttribute( QwtPolarCurve::LegendShowSymbol, true );

    switch( id )
    {
        case PlotSettings::Spiral:
        {
            curve->setTitle( "Spiral" );
            curve->setPen( QPen( Qt::yellow, 2 ) );
            curve->setSymbol( new QwtSymbol( QwtSymbol::Rect,
                QBrush( Qt::cyan ), QPen( Qt::white ), QSize( 3, 3 ) ) );
            curve->setData(
                new SpiralData( s_radialInterval, s_azimuthInterval, numPoints ) );
            break;
        }
        case PlotSettings::Rose:
        {
            curve->setTitle( "Rose" );
            curve->setPen( QPen( Qt::red, 2 ) );
            curve->setSymbol( new QwtSymbol( QwtSymbol::Rect,
                QBrush( Qt::cyan ), QPen( Qt::white ), QSize( 3, 3 ) ) );
            curve->setData(
                new RoseData( s_radialInterval, s_azimuthInterval, numPoints ) );
            break;
        }
    }
    return curve;
}

#include "moc_Plot.cpp"
