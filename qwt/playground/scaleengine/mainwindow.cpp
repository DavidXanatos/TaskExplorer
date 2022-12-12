/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "MainWindow.h"
#include "Plot.h"
#include "TransformPlot.h"

#include <QwtTransform>
#include <QwtMath>

#include <QSplitter>

namespace
{
    class TransformPos : public QwtTransform
    {
      public:
        TransformPos( double pos, double range, double factor )
            : m_position( pos )
            , m_range( range )
            , m_factor( factor )
            , m_powRange( std::pow( m_range, m_factor ) )
        {
        }

        virtual double transform( double value ) const QWT_OVERRIDE
        {
            const double v1 = m_position - m_range;
            const double v2 = v1 + 2 * m_range;

            if ( value <= v1 )
            {
                return value;
            }

            if ( value >= v2 )
            {
                return v1 + 2 * m_powRange + value - v2;
            }

            double v;

            if ( value <= m_position )
            {
                v = v1 + std::pow( value - v1, m_factor );
            }
            else
            {
                v = v1 + 2 * m_powRange - std::pow( v2 - value, m_factor );
            }

            return v;
        }

        virtual double invTransform( double value ) const QWT_OVERRIDE
        {
            const double v1 = m_position - m_range;
            const double v2 = v1 + 2 * m_powRange;

            if ( value < v1 )
            {
                return value;
            }

            if ( value >= v2 )
            {
                return value + 2 * ( m_range - m_powRange );
            }

            double v;
            if ( value <= v1 + m_powRange )
            {
                v = v1 + std::pow( value - v1, 1.0 / m_factor );
            }
            else
            {
                v = m_position + m_range - std::pow( v2 - value, 1.0 / m_factor );
            }

            return v;
        }

        virtual QwtTransform* copy() const QWT_OVERRIDE
        {
            return new TransformPos( m_position, m_range, m_factor );
        }

      private:
        const double m_position;
        const double m_range;
        const double m_factor;
        const double m_powRange;
    };
}

MainWindow::MainWindow( QWidget* parent ):
    QMainWindow( parent )
{
    QSplitter* splitter = new QSplitter( Qt::Vertical );

    m_transformPlot = new TransformPlot( splitter );

    m_transformPlot->insertTransformation( "Square Root",
        QColor( "DarkSlateGray" ), new QwtPowerTransform( 0.5 ) );
    m_transformPlot->insertTransformation( "Linear",
        QColor( "Peru" ), new QwtNullTransform() );
    m_transformPlot->insertTransformation( "Cubic",
        QColor( "OliveDrab" ), new QwtPowerTransform( 3.0 ) );
    m_transformPlot->insertTransformation( "Power 10",
        QColor( "Indigo" ), new QwtPowerTransform( 10.0 ) );
    m_transformPlot->insertTransformation( "Log",
        QColor( "SteelBlue" ), new QwtLogTransform() );
    m_transformPlot->insertTransformation( "At 400",
        QColor( "Crimson" ), new TransformPos( 400.0, 100.0, 1.4 ) );

    const QwtPlotItemList curves =
        m_transformPlot->itemList( QwtPlotItem::Rtti_PlotCurve );
    if ( !curves.isEmpty() )
        m_transformPlot->setLegendChecked( curves[ 2 ] );

    m_plot = new Plot( splitter );
    m_plot->setTransformation( new QwtPowerTransform( 3.0 ) );

    setCentralWidget( splitter );

    connect( m_transformPlot, SIGNAL(selected(QwtTransform*)),
        m_plot, SLOT(setTransformation(QwtTransform*)) );
}

#include "moc_MainWindow.cpp"
