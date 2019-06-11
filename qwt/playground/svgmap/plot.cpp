#include <qfiledialog.h>
#include <qwt_plot_svgitem.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_layout.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_panner.h>
#include <qwt_plot_magnifier.h>
#include "plot.h"

Plot::Plot( QWidget *parent ):
    QwtPlot( parent ),
    d_mapItem( NULL ),
    d_mapRect( 0.0, 0.0, 100.0, 100.0 ) // something
{
#if 1
    /*
       d_mapRect is only a reference for zooming, but
       the ranges are nothing useful for the user. So we
       hide the axes.
     */
    plotLayout()->setCanvasMargin( 0 );
    for ( int axis = 0; axis < QwtPlot::axisCnt; axis++ )
        enableAxis( axis, false );
#else
    QwtPlotGrid *grid = new QwtPlotGrid();
    grid->attach( this );
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

void Plot::loadSVG( const QString &fileName )
{
    if ( d_mapItem == NULL )
    {
        d_mapItem = new QwtPlotSvgItem();
        d_mapItem->attach( this );
    }

    d_mapItem->loadFile( d_mapRect, fileName );
    rescale();

    replot();
}

void Plot::rescale()
{
    setAxisScale( QwtPlot::xBottom,
        d_mapRect.left(), d_mapRect.right() );
    setAxisScale( QwtPlot::yLeft,
        d_mapRect.top(), d_mapRect.bottom() );
}
