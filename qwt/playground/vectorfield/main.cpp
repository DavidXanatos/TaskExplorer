/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include <QwtPlot>
#include <QwtVectorFieldSample>
#include <QwtPlotVectorField>
#include <QwtVectorFieldArrow>
#include <QwtPlotGrid>
#include <QwtLegend>
#include <QwtColorMap>

#include <QApplication>
#include <QPen>

namespace
{
    class VectorField : public QwtPlotVectorField
    {
      public:
        VectorField()
            : QwtPlotVectorField( "Vector Field" )
        {
            setRenderHint( QwtPlotItem::RenderAntialiased, true );
            setLegendIconSize( QSize( 20, 10 ) );

            setMagnitudeScaleFactor( 1.0 );

#if 0
            // test solid arrow
            setSymbol( new QwtVectorFieldArrow() );
            setPen( Qt::NoPen );
            setBrush( Qt::black );
            setMagnitudeScaleFactor( 2 );
#endif

#if 1
            // test color map
            QwtLinearColorMap* cm = new QwtLinearColorMap();
            cm->setColorInterval( Qt::yellow, Qt::blue );
            cm->addColorStop( 0.5, Qt::red );
            setColorMap( cm );
            setMagnitudeMode( MagnitudeAsColor, true );
#endif

#if 1
            setIndicatorOrigin( QwtPlotVectorField::OriginHead );
#else
            setIndicatorOrigin( QwtPlotVectorField::OriginTail );
#endif

#if 0
            setRasterSize( QSizeF( 20, 20 ) );
#endif
            setSamples( samples() );
        }

      private:
        QVector< QwtVectorFieldSample > samples() const
        {
            const int dim = 10;

            QVector< QwtVectorFieldSample > samples;

            for ( int x = -dim; x < dim; x++ )
            {
                for ( int y = -dim; y < dim; y++ )
                {
                    samples += QwtVectorFieldSample( x, y, y, -x );
                }
            }

            return samples;
        }
    };

    class Plot : public QwtPlot
    {
      public:
        Plot()
        {
            setTitle( "Vector Field" );
            setCanvasBackground( Qt::white );

            insertLegend( new QwtLegend() );

            QwtPlotGrid* grid = new QwtPlotGrid();
            grid->attach( this );

            VectorField* fieldItem = new VectorField();
            fieldItem->attach( this );

            const QRectF r = fieldItem->boundingRect();

#if 1
            setAxisScale( QwtAxis::XBottom, r.left() - 1.0, r.right() + 1.0 );
#else
            setAxisScale( QwtAxis::XBottom, r.right() + 1.0, r.left() - 1.0 );
#endif

#if 1
            setAxisScale( QwtAxis::YLeft, r.top() - 1.0, r.bottom() + 1.0 );
#else
            setAxisScale( QwtAxis::YLeft, r.bottom() + 1.0, r.top() - 1.0 );
#endif
        }
    };
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    Plot plot;
    plot.resize( 600, 400 );
    plot.show();

    return app.exec();
}
