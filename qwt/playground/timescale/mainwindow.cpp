#include "plot.h"
#include "panel.h"
#include "mainwindow.h"
#include <qwt_date.h>
#include <qwt_scale_widget.h>
#include <qlayout.h>

MainWindow::MainWindow( QWidget *parent ):
    QMainWindow( parent )
{
    Settings settings;
#if 1
    settings.startDateTime = QDateTime( QDate( 2012, 10, 27 ), QTime( 18, 5, 0, 0 ) );
    settings.endDateTime = QDateTime( QDate( 2012, 10, 28 ), QTime( 12, 12, 0, 0 ) );
#else
    settings.startDateTime = QDateTime( QDate( 2011, 5, 3 ), QTime( 0, 6, 0, 0 ) );
    settings.endDateTime = QDateTime( QDate( 2012, 3, 10 ), QTime( 0, 5, 0, 0 ) );
#endif
    settings.maxMajorSteps = 10;
    settings.maxMinorSteps = 8;
    settings.maxWeeks = -1;

    d_plot = new Plot();
    d_panel = new Panel();
    d_panel->setSettings( settings );

    QWidget *box = new QWidget( this );

    QHBoxLayout *layout = new QHBoxLayout( box );
    layout->addWidget( d_plot, 10 );
    layout->addWidget( d_panel );

    setCentralWidget( box );

    updatePlot();

    connect( d_panel, SIGNAL( edited() ), SLOT( updatePlot() ) );
    connect( d_plot->axisWidget( QwtPlot::yLeft ), 
        SIGNAL( scaleDivChanged() ), SLOT( updatePanel() ) );
}

void MainWindow::updatePlot()
{
    d_plot->blockSignals( true );
    d_plot->applySettings( d_panel->settings() );
    d_plot->blockSignals( false );
}

void MainWindow::updatePanel()
{
    const QwtScaleDiv scaleDiv = d_plot->axisScaleDiv( QwtPlot::yLeft );

    Settings settings = d_panel->settings();
    settings.startDateTime = QwtDate::toDateTime( scaleDiv.lowerBound(), Qt::LocalTime );
    settings.endDateTime = QwtDate::toDateTime( scaleDiv.upperBound(), Qt::LocalTime );

    d_panel->setSettings( settings );
}
