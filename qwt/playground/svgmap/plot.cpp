/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QwtPlotGraphicItem>
#include <QwtPlotLayout>
#include <QwtPlotPanner>
#include <QwtPlotMagnifier>
#include <QwtGraphic>

#define DEBUG_SCALE 0

#if DEBUG_SCALE
#include <QwtPlotGrid>
#endif

#include <QSvgRenderer>
#include <QFileDialog>

Plot::Plot( QWidget* parent )
    : QwtPlot( parent )
    , m_mapItem( NULL )
    , m_mapRect( 0.0, 0.0, 100.0, 100.0 ) // something
{
#if DEBUG_SCALE
    QwtPlotGrid* grid = new QwtPlotGrid();
    grid->attach( this );
#else
    /*
       m_mapRect is only a reference for zooming, but
       the ranges are nothing useful for the user. So we
       hide the axes.
     */
    plotLayout()->setCanvasMargin( 0 );
    for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
        setAxisVisible( axisPos, false );
#endif

    /*
       Navigation:

       Left Mouse Button: Panning
       Mouse Wheel:       Zooming In/Out
       Right Mouse Button: Reset to initial
     */

    ( void )new QwtPlotPanner( canvas() );
    ( void )new QwtPlotMagnifier( canvas() );

    canvas()->setFocusPolicy( Qt::WheelFocus );
    setCanvasBackground( Qt::white );

    rescale();
}

#ifndef QT_NO_FILEDIALOG

void Plot::loadSVG()
{
    QString dir;
    const QString fileName = QFileDialog::getOpenFileName( NULL,
        "Load a Scaleable Vector Graphic (SVG) Map",
        dir, "SVG Files (*.svg)" );

    if ( !fileName.isEmpty() )
        loadSVG( fileName );
}

#endif

void Plot::loadSVG( const QString& fileName )
{
    if ( m_mapItem == NULL )
    {
        m_mapItem = new QwtPlotGraphicItem();
        m_mapItem->attach( this );
    }

    QwtGraphic graphic;

    QSvgRenderer renderer;
    if ( renderer.load( fileName ) )
    {
        QPainter p( &graphic );
        renderer.render( &p );
    }

    m_mapItem->setGraphic( m_mapRect, graphic );

    rescale();
    replot();
}

void Plot::rescale()
{
    setAxisScale( QwtAxis::XBottom, m_mapRect.left(), m_mapRect.right() );
    setAxisScale( QwtAxis::YLeft, m_mapRect.top(), m_mapRect.bottom() );
}

#include "moc_Plot.cpp"
