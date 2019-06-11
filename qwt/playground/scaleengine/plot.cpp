#include "plot.h"
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_symbol.h>
#include <qwt_plot_picker.h>
#include <qwt_scale_engine.h>

Plot::Plot( QWidget *parent ):
    QwtPlot( parent )
{
    setCanvasBackground( Qt::white );

    setAxisScale(QwtPlot::yLeft, 0.0, 10.0 );
    setTransformation( new QwtNullTransform() );

    populate();

    QwtPlotPicker *picker = new QwtPlotPicker( canvas() );
    picker->setTrackerMode( QwtPlotPicker::AlwaysOn );
}

void Plot::populate()
{
    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->setMinorPen( Qt::black, 0, Qt::DashLine );
    grid->enableXMin( true );
    grid->attach( this );

    QwtPlotCurve *curve = new QwtPlotCurve();
    curve->setTitle("Some Points");
    curve->setPen( Qt::blue, 4 ),
    curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

    QwtSymbol *symbol = new QwtSymbol( QwtSymbol::Ellipse, 
        QBrush( Qt::yellow ), QPen( Qt::red, 2 ), QSize( 8, 8 ) );
    curve->setSymbol( symbol );

    QPolygonF points;
    points << QPointF( 10.0, 4.4 ) 
        << QPointF( 100.0, 3.0 ) << QPointF( 200.0, 4.5 ) 
        << QPointF( 300.0, 6.8 ) << QPointF( 400.0, 7.9 ) 
        << QPointF( 500.0, 7.1 ) << QPointF( 600.0, 7.9 ) 
        << QPointF( 700.0, 7.1 ) << QPointF( 800.0, 5.4 ) 
        << QPointF( 900.0, 2.8 ) << QPointF( 1000.0, 3.6 );
    curve->setSamples( points );
    curve->attach( this );
}

void Plot::setTransformation( QwtTransform *transform )
{
    QwtLinearScaleEngine *scaleEngine = new QwtLinearScaleEngine();
    scaleEngine->setTransformation( transform );

    setAxisScaleEngine( QwtPlot::xBottom, scaleEngine );

    // we have to reassign the axis settinge, because they are
    // invalidated, when the scale engine has changed

    QwtScaleDiv scaleDiv = 
        axisScaleEngine( QwtPlot::xBottom )->divideScale( 10.0, 1000.0, 8, 10 );

    QList<double> ticks;
    ticks += 10.0;
    ticks += scaleDiv.ticks( QwtScaleDiv::MajorTick );
    scaleDiv.setTicks( QwtScaleDiv::MajorTick, ticks );

    setAxisScaleDiv( QwtPlot::xBottom, scaleDiv );

    replot();
}
