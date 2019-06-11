#include "plot.h"
#include "curvetracker.h"
#include <qwt_picker_machine.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_textlabel.h>
#include <qwt_plot_zoneitem.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_layout.h>
#include <qwt_scale_widget.h>
#include <qwt_symbol.h>

Plot::Plot( QWidget *parent ):
    QwtPlot( parent)
{
    setPalette( Qt::black );

    // we want to have the axis scales like a frame around the
    // canvas
    plotLayout()->setAlignCanvasToScales( true );
    for ( int axis = 0; axis < QwtPlot::axisCnt; axis++ )
        axisWidget( axis )->setMargin( 0 );

    QwtPlotCanvas *canvas = new QwtPlotCanvas();
    canvas->setAutoFillBackground( false );
    canvas->setFrameStyle( QFrame::NoFrame );
    setCanvas( canvas );

    setAxisScale( QwtPlot::yLeft, 0.0, 10.0 );

    // a title 
    QwtText title( "Picker Demo" );
    title.setColor( Qt::white );
    title.setRenderFlags( Qt::AlignHCenter | Qt::AlignTop );

    QFont font;
    font.setBold( true );
    title.setFont( font );

    QwtPlotTextLabel *titleItem = new QwtPlotTextLabel();
    titleItem->setText( title );
    titleItem->attach( this );

#if 1
    // section

    //QColor c( "PaleVioletRed" );

    QwtPlotZoneItem* zone = new QwtPlotZoneItem();
    zone->setPen( Qt::darkGray );
    zone->setBrush( QColor( "#834358" ) );
    zone->setOrientation( Qt::Horizontal );
    zone->setInterval( 3.8, 5.7 );
    zone->attach( this );

#else
    // grid

    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->setMajorPen( Qt::white, 0, Qt::DotLine );
    grid->setMinorPen( Qt::gray, 0 , Qt::DotLine );
    grid->attach( this );
#endif

    // curves

    QPolygonF points1;
    points1 << QPointF( 0.2, 4.4 ) << QPointF( 1.2, 3.0 )
        << QPointF( 2.7, 4.5 ) << QPointF( 3.5, 6.8 )
        << QPointF( 4.7, 7.9 ) << QPointF( 5.8, 7.1 );

    insertCurve( "Curve 1", "DarkOrange", points1 );

    QPolygonF points2;
    points2 << QPointF( 0.4, 8.7 ) << QPointF( 1.4, 7.8 )
        << QPointF( 2.3, 5.5 ) << QPointF( 3.3, 4.1 )
        << QPointF( 4.4, 5.2 ) << QPointF( 5.6, 5.7 );

    insertCurve( "Curve 2", "DodgerBlue", points2 );

    CurveTracker* tracker = new CurveTracker( this->canvas() );

    // for the demo we want the tracker to be active without
    // having to click on the canvas
    tracker->setStateMachine( new QwtPickerTrackerMachine() );
    tracker->setRubberBandPen( QPen( "MediumOrchid" ) );
}

void Plot::insertCurve( const QString &title, 
    const QColor &color, const QPolygonF &points )
{
    QwtPlotCurve *curve = new QwtPlotCurve();
    curve->setTitle( title );
    curve->setPen( color, 2 ),
    curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );

    QwtSymbol *symbol = new QwtSymbol( QwtSymbol::Ellipse,
        QBrush( Qt::white ), QPen( color, 2 ), QSize( 8, 8 ) );
    curve->setSymbol( symbol );

    curve->setSamples( points );

    curve->attach( this );
}


