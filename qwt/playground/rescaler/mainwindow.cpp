#include <cstdlib>
#include <qgroupbox.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qstatusbar.h>
#include <qlabel.h>
#include <qwt_plot.h>
#include <qwt_plot_rescaler.h>
#include <qwt_scale_div.h>
#include "plot.h"
#include "mainwindow.h"

MainWindow::MainWindow()
{
    QFrame *w = new QFrame( this );

    QWidget *panel = createPanel( w );
    panel->setFixedWidth( 2 * panel->sizeHint().width() );
    d_plot = createPlot( w );

    QHBoxLayout *layout = new QHBoxLayout( w );
    layout->setMargin( 0 );
    layout->addWidget( panel, 0 );
    layout->addWidget( d_plot, 10 );

    setCentralWidget( w );

    setRescaleMode( 0 );

    ( void )statusBar();
}

QWidget *MainWindow::createPanel( QWidget *parent )
{
    QGroupBox *panel = new QGroupBox( "Navigation Panel", parent );

    QComboBox *rescaleBox = new QComboBox( panel );
    rescaleBox->setEditable( false );
    rescaleBox->insertItem( KeepScales, "None" );
    rescaleBox->insertItem( Fixed, "Fixed" );
    rescaleBox->insertItem( Expanding, "Expanding" );
    rescaleBox->insertItem( Fitting, "Fitting" );

    connect( rescaleBox, SIGNAL( activated( int ) ), SLOT( setRescaleMode( int ) ) );

    d_rescaleInfo = new QLabel( panel );
    d_rescaleInfo->setSizePolicy(
        QSizePolicy::Expanding, QSizePolicy::Expanding );
    d_rescaleInfo->setWordWrap( true );

    QVBoxLayout *layout = new QVBoxLayout( panel );
    layout->addWidget( rescaleBox );
    layout->addWidget( d_rescaleInfo );
    layout->addStretch( 10 );

    return panel;
}

Plot *MainWindow::createPlot( QWidget *parent )
{
    Plot *plot = new Plot( parent, QwtInterval( 0.0, 1000.0 ) );
    plot->replot();

    d_rescaler = new QwtPlotRescaler( plot->canvas() );
    d_rescaler->setReferenceAxis( QwtPlot::xBottom );
    d_rescaler->setAspectRatio( QwtPlot::yLeft, 1.0 );
    d_rescaler->setAspectRatio( QwtPlot::yRight, 0.0 );
    d_rescaler->setAspectRatio( QwtPlot::xTop, 0.0 );

    for ( int axis = 0; axis < QwtPlot::axisCnt; axis++ )
        d_rescaler->setIntervalHint( axis, QwtInterval( 0.0, 1000.0 ) );

    connect( plot, SIGNAL( resized( double, double ) ),
        SLOT( showRatio( double, double ) ) );
    return plot;
}

void MainWindow::setRescaleMode( int mode )
{
    bool doEnable = true;
    QString info;
    QRectF rectOfInterest;
    QwtPlotRescaler::ExpandingDirection direction = QwtPlotRescaler::ExpandUp;

    switch( mode )
    {
        case KeepScales:
        {
            doEnable = false;
            info = "All scales remain unchanged, when the plot is resized";
            break;
        }
        case Fixed:
        {
            d_rescaler->setRescalePolicy( QwtPlotRescaler::Fixed );
            info = "The scale of the bottom axis remains unchanged, "
                "when the plot is resized. All other scales are changed, "
                "so that a pixel on screen means the same distance for"
                "all scales.";
            break;
        }
        case Expanding:
        {
            d_rescaler->setRescalePolicy( QwtPlotRescaler::Expanding );
            info = "The scales of all axis are shrinked/expanded, when "
                "resizing the plot, keeping the distance that is represented "
                "by one pixel.";
            d_rescaleInfo->setText( "Expanding" );
            break;
        }
        case Fitting:
        {
            d_rescaler->setRescalePolicy( QwtPlotRescaler::Fitting );
            const QwtInterval xIntv =
                d_rescaler->intervalHint( QwtPlot::xBottom );
            const QwtInterval yIntv =
                d_rescaler->intervalHint( QwtPlot::yLeft );

            rectOfInterest = QRectF( xIntv.minValue(), yIntv.minValue(),
                xIntv.width(), yIntv.width() );
            direction = QwtPlotRescaler::ExpandBoth;

            info = "Fitting";
            break;
        }
    }

    d_plot->setRectOfInterest( rectOfInterest );

    d_rescaleInfo->setText( info );
    d_rescaler->setEnabled( doEnable );
    for ( int axis = 0; axis < QwtPlot::axisCnt; axis++ )
        d_rescaler->setExpandingDirection( direction );

    if ( doEnable )
        d_rescaler->rescale();
    else
        d_plot->replot();
}

void MainWindow::showRatio( double xRatio, double yRatio )
{
    const QString msg = QString( "%1, %2" ).arg( xRatio ).arg( yRatio );
    statusBar()->showMessage( msg );
}

