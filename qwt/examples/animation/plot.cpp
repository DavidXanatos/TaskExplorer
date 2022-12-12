/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "Plot.h"

#include <QwtMath>
#include <QwtSymbol>
#include <QwtPlotCurve>
#include <QwtPlotLayout>

class Curve : public QwtPlotCurve
{
  public:
    void setTransformation( const QTransform& transform )
    {
        m_transform = transform;
    }

    virtual void updateSamples( double phase )
    {
        setSamples( m_transform.map( points( phase ) ) );
    }

  private:
    virtual QPolygonF points( double phase ) const = 0;

  private:
    QTransform m_transform;
};

class Curve1 : public Curve
{
  public:
    Curve1()
    {
        setPen( QColor( 150, 150, 200 ), 2 );
        setStyle( QwtPlotCurve::Lines );

        setCurveAttribute( QwtPlotCurve::Fitted, true );

        QwtSymbol* symbol = new QwtSymbol( QwtSymbol::XCross );
        symbol->setPen( Qt::yellow );
        symbol->setSize( 7 );

        setSymbol( symbol );

        // somewhere to the left
        QTransform transform;
        transform.scale( 1.5, 1.0 );
        transform.translate( 1.5, 3.0 );

        setTransformation( transform );
    }

    virtual QPolygonF points( double phase ) const QWT_OVERRIDE
    {
        QPolygonF points;

        const int numSamples = 15;
        for ( int i = 0; i < numSamples; i++ )
        {
            const double v = 6.28 * double( i ) / double( numSamples - 1 );
            points += QPointF( std::sin( v - phase ), v );
        }

        return points;
    }
};

class Curve2 : public Curve
{
  public:
    Curve2()
    {
        setStyle( QwtPlotCurve::Sticks );
        setPen( QColor( 200, 150, 50 ) );

        setSymbol( new QwtSymbol( QwtSymbol::Ellipse,
            QColor( Qt::gray ), QColor( Qt::yellow ), QSize( 5, 5 ) ) );
    }

  private:
    virtual QPolygonF points( double phase ) const QWT_OVERRIDE
    {
        QPolygonF points;

        const int numSamples = 50;
        for ( int i = 0; i < numSamples; i++ )
        {
            const double v = 10.0 * i / double( numSamples - 1 );
            points += QPointF( v, std::cos( 3.0 * ( v + phase ) ) );
        }

        return points;
    }
};

class Curve3 : public Curve
{
  public:
    Curve3()
    {
        setStyle( QwtPlotCurve::Lines );
        setPen( QColor( 100, 200, 150 ), 2 );

        setCurveAttribute( QwtPlotCurve::Fitted, true );

        // somewhere in the top right corner
        QTransform transform;
        transform.translate( 7.0, 7.5 );
        transform.scale( 2.0, 2.0 );

        setTransformation( transform );
    }

  private:
    virtual QPolygonF points( double phase ) const QWT_OVERRIDE
    {
        QPolygonF points;

        const int numSamples = 9;
        for ( int i = 0; i < numSamples; i++ )
        {
            const double v = i * 2.0 * M_PI / ( numSamples - 1 );
            points += QPointF( std::sin( v - phase ), std::cos( 3.0 * ( v + phase ) ) );
        }

        return points;
    }
};

class Curve4 : public Curve
{
  public:
    Curve4()
    {
        setStyle( QwtPlotCurve::Lines );
        setPen( Qt::red, 2 );

        initSamples();

        // somewhere in the center
        QTransform transform;
        transform.translate( 7.0, 3.0 );
        transform.scale( 1.5, 1.5 );

        setTransformation( transform );
    }

  private:
    virtual QPolygonF points( double phase ) const QWT_OVERRIDE
    {
        const double speed = 0.05;

        const double s = speed * std::sin( phase );
        const double c = std::sqrt( 1.0 - s * s );

        for ( int i = 0; i < m_points.size(); i++ )
        {
            const QPointF p = m_points[i];

            const double u = p.x();
            const double v = p.y();

            m_points[i].setX( u * c - v * s );
            m_points[i].setY( v * c + u * s );
        }

        return m_points;
    }

    void initSamples()
    {
        const int numSamples = 15;

        for ( int i = 0; i < numSamples; i++ )
        {
            const double angle = i * ( 2.0 * M_PI / ( numSamples - 1 ) );

            QPointF p( std::cos( angle ), std::sin( angle ) );
            if ( i % 2 )
                p *= 0.4;

            m_points += p;
        }
    }

  private:
    mutable QPolygonF m_points;
};

Plot::Plot( QWidget* parent )
    : QwtPlot( parent)
{
    setAutoReplot( false );

    setTitle( "Animated Curves" );

    // hide all axes
    for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
        setAxisVisible( axisPos, false );

    plotLayout()->setCanvasMargin( 10 );

    Curve* curve1 = new Curve1();
    curve1->attach( this );

    Curve* curve2 = new Curve2();
    curve2->attach( this );

    Curve* curve3 = new Curve3();
    curve3->attach( this );

    Curve* curve4 = new Curve4();
    curve4->attach( this );

    updateCurves();

    m_timer.start();
    ( void )startTimer( 40 );
}

void Plot::timerEvent( QTimerEvent* )
{
    updateCurves();
    replot();
}

void Plot::updateCurves()
{
    const double speed = 2 * M_PI / 25000.0; // a cycle every 25 seconds
    const double phase = m_timer.elapsed() * speed;

    const QwtPlotItemList& items = itemList();
    for ( QwtPlotItemIterator it = items.constBegin();
        it != items.constEnd(); ++it )
    {
        QwtPlotItem* item = *it;

        if ( item->rtti() == QwtPlotItem::Rtti_PlotCurve )
        {
            Curve* curve = dynamic_cast< Curve* >( item );
            curve->updateSamples( phase );
        }
    }
}
