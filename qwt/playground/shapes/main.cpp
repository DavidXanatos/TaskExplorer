/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include <QwtPlot>
#include <QwtLegend>
#include <QwtPlotPanner>
#include <QwtPlotMagnifier>
#include <QwtPlotShapeItem>

#include <QApplication>
#include <QPainterPath>

namespace
{
    class ShapeItem : public QwtPlotShapeItem
    {
      public:
        ShapeItem()
            : QwtPlotShapeItem( "Shape" )
        {
            setRenderHint( QwtPlotItem::RenderAntialiased, true );
            setRenderTolerance( 1.0 );

            setPen( Qt::yellow );

            QColor c = Qt::darkRed;
            c.setAlpha( 100 );
            setBrush( c );

            setShape( demoShape() );
        }

      private:
        static QPainterPath demoShape()
        {
            const double d = 900.0;
            const QRectF rect( 1.0, 1.0, d, d );

            QPainterPath path;
            //path.setFillRule( Qt::WindingFill );
            path.addEllipse( rect );

            const QRectF rect2 = rect.adjusted( 0.2 * d, 0.3 * d, -0.22 * d, 1.5 * d );
            path.addEllipse( rect2 );

#if 0
            QFont font;
            font.setPointSizeF( 200 );
            QPainterPath textPath;
            textPath.addText( rect.center(), font, "Shape" );

            QTransform transform;
            transform.translate( rect.center().x() - 600, rect.center().y() + 50 );
            transform.rotate( 180.0, Qt::XAxis );

            textPath = transform.map( textPath );

            path.addPath( textPath );
#endif
            return path;
        }
    };

    class Plot : public QwtPlot
    {
      public:
        Plot()
        {
            setPalette( QColor( 60, 60, 60 ) );
            canvas()->setPalette( Qt::white );

            // panning with the left mouse button
            ( void ) new QwtPlotPanner( canvas() );

            // zoom in/out with the wheel
            ( void ) new QwtPlotMagnifier( canvas() );

            setTitle( "Shapes" );
            insertLegend( new QwtLegend(), QwtPlot::RightLegend );

            // axes
            using namespace QwtAxis;

            setAxisTitle( XBottom, "x -->" );
            setAxisTitle( YLeft, "y -->" );
#if 0
            setAxisScaleEngine( XBottom, new QwtLog10ScaleEngine );
            setAxisScaleEngine( YLeft, new QwtLog10ScaleEngine );
#endif

            ShapeItem* item = new ShapeItem();
            item->setItemAttribute( QwtPlotItem::Legend, true );
            item->attach( this );
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
