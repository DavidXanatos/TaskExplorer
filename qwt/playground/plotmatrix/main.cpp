#include "plotmatrix.h"
#include <qwt_plot_grid.h>
#include <qwt_scale_widget.h>
#include <qapplication.h>
#include <qpen.h>
#include <qmath.h>

class MainWindow: public PlotMatrix
{
public:
    MainWindow();
};

MainWindow::MainWindow():
    PlotMatrix( 3, 4 )
{
    enableAxis( QwtPlot::yLeft );
    enableAxis( QwtPlot::yRight );
    enableAxis( QwtPlot::xBottom );

    for ( int row = 0; row < numRows(); row++ )
    {
        const double v = qPow( 10.0, row );
        setAxisScale( QwtPlot::yLeft, row, -v, v );
        setAxisScale( QwtPlot::yRight, row, -v, v );
    }

    for ( int col = 0; col < numColumns(); col++ )
    {
        const double v = qPow( 10.0, col );
        setAxisScale( QwtPlot::xBottom, col, -v, v );
        setAxisScale( QwtPlot::xTop, col, -v, v );
    }

    for ( int row = 0; row < numRows(); row++ )
    {
        for ( int col = 0; col < numColumns(); col++ )
        {
            QwtPlot *plot = plotAt( row, col );
            plot->setCanvasBackground( QColor( Qt::darkGray ) );

            QwtPlotGrid *grid = new QwtPlotGrid();
            grid->enableXMin( true );
            grid->setMajorPen( Qt::white, 0, Qt::DotLine );
            grid->setMinorPen( Qt::gray, 0 , Qt::DotLine );
            grid->attach( plot );
        }
    }

    plotAt( 1, 0 )->axisWidget( QwtPlot::yLeft )->setLabelRotation( 45 );
    plotAt( 1, numColumns() - 1 )->axisWidget( QwtPlot::yRight )->setLabelRotation( -45 );

    updateLayout();
}

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    MainWindow mainWindow;

    mainWindow.resize( 800, 600 );
    mainWindow.show();

    return a.exec();
}
