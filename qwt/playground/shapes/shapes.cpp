#include <qapplication.h>
#include <qpainterpath.h>
#include <qwt_plot.h>
#include <qwt_legend.h>
#include <qwt_point_data.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_magnifier.h>
#include <qwt_plot_shapeitem.h>
#include <qwt_scale_engine.h>

class Plot : public QwtPlot
{
public:
    Plot( QWidget *parent = NULL );

private:
    void populate();
};


Plot::Plot( QWidget *parent ):
    QwtPlot( parent )
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
    setAxisTitle( xBottom, "x -->" );
    setAxisTitle( yLeft, "y -->" );
#if 0
    setAxisScaleEngine( xBottom, new QwtLog10ScaleEngine );
    setAxisScaleEngine( yLeft, new QwtLog10ScaleEngine );
#endif

    populate();
}

void Plot::populate()
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
    textPath.addText( rect.center(), font, "Seppi" );

    QTransform transform;
    transform.translate( rect.center().x() - 600, rect.center().y() + 50 );
    transform.rotate( 180.0, Qt::XAxis );

    textPath = transform.map( textPath );

    path.addPath( textPath );
#endif

    QwtPlotShapeItem *item = new QwtPlotShapeItem( "Shape" );
    item->setItemAttribute( QwtPlotItem::Legend, true );
    item->setRenderHint( QwtPlotItem::RenderAntialiased, true );
#if 1
    item->setRenderTolerance( 1.0 );
#endif
    item->setShape( path );
    item->setPen( Qt::yellow );

    QColor c = Qt::darkRed;
    c.setAlpha( 100 );
    item->setBrush( c );

    item->attach( this );
}

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    Plot plot;
    plot.resize( 600, 400 );
    plot.show();

    return a.exec();
}
