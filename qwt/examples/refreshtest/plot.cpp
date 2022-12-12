/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "CircularBuffer.h"
#include "Settings.h"

#include <QwtPainter>
#include <QwtPlotCanvas>
#include <QwtPlotGrid>
#include <QwtPlotCurve>
#include <QwtPlotLayout>
#include <QwtScaleWidget>
#include <QwtScaleDraw>
#include <QwtMath>

#ifndef QWT_NO_OPENGL

#if QT_VERSION >= 0x050400
#define USE_OPENGL_WIDGET 1
#endif

#if USE_OPENGL_WIDGET
    #include <QwtPlotOpenGLCanvas>
#else
    #include <QwtPlotGLCanvas>
#endif
#endif

static double wave( double x )
{
    const double period = 1.0;
    const double c = 5.0;

    double v = std::fmod( x, period );

    const double amplitude = qAbs( x - qRound( x / c ) * c ) / ( 0.5 * c );
    v = amplitude * std::sin( v / period * 2 * M_PI );

    return v;
}

static double noise( double )
{
    return 2.0 * ( qwtRand() / ( static_cast< double >( RAND_MAX ) + 1 ) ) - 1.0;
}

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
    , m_interval( 10.0 ) // seconds
    , m_timerId( -1 )
{
    // Assign a title
    setTitle( "Testing Refresh Rates" );

    QwtPlotCanvas* canvas = new QwtPlotCanvas();
    canvas->setFrameStyle( QFrame::Box | QFrame::Plain );
    canvas->setLineWidth( 1 );
    canvas->setPalette( Qt::white );

    setCanvas( canvas );

    alignScales();

    // Insert grid
    m_grid = new QwtPlotGrid();
    m_grid->attach( this );

    // Insert curve
    m_curve = new QwtPlotCurve( "Data Moving Right" );
    m_curve->setPen( Qt::black );
    m_curve->setData( new CircularBuffer( m_interval, 10 ) );
    m_curve->attach( this );

    // Axis
    setAxisTitle( QwtAxis::XBottom, "Seconds" );
    setAxisScale( QwtAxis::XBottom, -m_interval, 0.0 );

    setAxisTitle( QwtAxis::YLeft, "Values" );
    setAxisScale( QwtAxis::YLeft, -1.0, 1.0 );

    m_elapsedTimer.start();

    setSettings( m_settings );
}

//
//  Set a plain canvas frame and align the scales to it
//
void Plot::alignScales()
{
    // The code below shows how to align the scales to
    // the canvas frame, but is also a good example demonstrating
    // why the spreaded API needs polishing.

    for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
    {
        QwtScaleWidget* scaleWidget = axisWidget( axisPos );
        if ( scaleWidget )
            scaleWidget->setMargin( 0 );

        QwtScaleDraw* scaleDraw = axisScaleDraw( axisPos );
        if ( scaleDraw )
            scaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
    }

    plotLayout()->setAlignCanvasToScales( true );
}

void Plot::setSettings( const Settings& s )
{
    if ( m_timerId >= 0 )
        killTimer( m_timerId );

    m_timerId = startTimer( s.updateInterval );

    m_grid->setPen( s.grid.pen );
    m_grid->setVisible( s.grid.pen.style() != Qt::NoPen );

    CircularBuffer* buffer = static_cast< CircularBuffer* >( m_curve->data() );
    if ( s.curve.numPoints != buffer->size() ||
        s.curve.functionType != m_settings.curve.functionType )
    {
        switch( s.curve.functionType )
        {
            case Settings::Wave:
                buffer->setFunction( wave );
                break;
            case Settings::Noise:
                buffer->setFunction( noise );
                break;
            default:
                buffer->setFunction( NULL );
        }

        buffer->fill( m_interval, s.curve.numPoints );
    }

    m_curve->setPen( s.curve.pen );
    m_curve->setBrush( s.curve.brush );

    m_curve->setPaintAttribute( QwtPlotCurve::ClipPolygons,
        s.curve.paintAttributes & QwtPlotCurve::ClipPolygons );
    m_curve->setPaintAttribute( QwtPlotCurve::FilterPoints,
        s.curve.paintAttributes & QwtPlotCurve::FilterPoints );
    m_curve->setPaintAttribute( QwtPlotCurve::FilterPointsAggressive,
        s.curve.paintAttributes & QwtPlotCurve::FilterPointsAggressive );

    m_curve->setRenderHint( QwtPlotItem::RenderAntialiased,
        s.curve.renderHint & QwtPlotItem::RenderAntialiased );

#ifndef QWT_NO_OPENGL
    if ( s.canvas.openGL )
    {
#if USE_OPENGL_WIDGET
        QwtPlotOpenGLCanvas* plotCanvas = qobject_cast< QwtPlotOpenGLCanvas* >( canvas() );
        if ( plotCanvas == NULL )
        {
            plotCanvas = new QwtPlotOpenGLCanvas();
            plotCanvas->setPalette( QColor( "NavajoWhite" ) );
            plotCanvas->setPalette( QColor( "khaki" ) );
            plotCanvas->setFrameStyle( QFrame::Box | QFrame::Plain );
            plotCanvas->setLineWidth( 1 );

            setCanvas( plotCanvas );
        }
#else
        QwtPlotGLCanvas* plotCanvas = qobject_cast< QwtPlotGLCanvas* >( canvas() );
        if ( plotCanvas == NULL )
        {
            plotCanvas = new QwtPlotGLCanvas();
            plotCanvas->setPalette( QColor( "khaki" ) );
            plotCanvas->setFrameStyle( QFrame::Box | QFrame::Plain );
            plotCanvas->setLineWidth( 1 );

            setCanvas( plotCanvas );
        }
#endif
    }
    else
#endif
    {
        QwtPlotCanvas* plotCanvas = qobject_cast< QwtPlotCanvas* >( canvas() );
        if ( plotCanvas == NULL )
        {
            plotCanvas = new QwtPlotCanvas();
            plotCanvas->setFrameStyle( QFrame::Box | QFrame::Plain );
            plotCanvas->setLineWidth( 1 );
            plotCanvas->setPalette( Qt::white );

            setCanvas( plotCanvas );
        }

        plotCanvas->setAttribute( Qt::WA_PaintOnScreen, s.canvas.paintOnScreen );

        plotCanvas->setPaintAttribute(
            QwtPlotCanvas::BackingStore, s.canvas.useBackingStore );
        plotCanvas->setPaintAttribute(
            QwtPlotCanvas::ImmediatePaint, s.canvas.immediatePaint );
    }

    QwtPainter::setPolylineSplitting( s.curve.lineSplitting );

    m_settings = s;
}

void Plot::timerEvent( QTimerEvent* )
{
    CircularBuffer* buffer = static_cast< CircularBuffer* >( m_curve->data() );
    buffer->setReferenceTime( m_elapsedTimer.elapsed() / 1000.0 );

    if ( m_settings.updateType == Settings::RepaintCanvas )
    {
        // the axes in this example doesn't change. So all we need to do
        // is to repaint the canvas.

        QMetaObject::invokeMethod( canvas(), "replot", Qt::DirectConnection );
    }
    else
    {
        replot();
    }
}

#include "moc_Plot.cpp"
