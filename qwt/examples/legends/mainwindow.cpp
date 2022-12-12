/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "Panel.h"
#include "MainWindow.h"

#include <QwtPlotRenderer>

#include <QToolBar>
#include <QToolButton>
#include <QLayout>

MainWindow::MainWindow( QWidget* parent )
    : QMainWindow( parent )
{
    m_plot = new Plot();

    Settings settings;
    settings.legend.isEnabled = true;
    settings.legend.position = QwtPlot::BottomLegend;

    settings.legendItem.isEnabled = false;
    settings.legendItem.numColumns = 1;
    settings.legendItem.alignment = Qt::AlignRight | Qt::AlignVCenter;
    settings.legendItem.backgroundMode = 0;
    settings.legendItem.size = m_plot->canvas()->font().pointSize();

    settings.curve.numCurves = 4;
    settings.curve.title = "Curve";

    m_panel = new Panel();
    m_panel->setSettings( settings );

    QWidget* box = new QWidget( this );
    QHBoxLayout* layout = new QHBoxLayout( box );
    layout->addWidget( m_plot, 10 );
    layout->addWidget( m_panel );

    setCentralWidget( box );

    QToolBar* toolBar = new QToolBar( this );

    QToolButton* btnExport = new QToolButton( toolBar );
    btnExport->setText( "Export" );
    toolBar->addWidget( btnExport );

    addToolBar( toolBar );

    updatePlot();

    connect( m_panel, SIGNAL(edited()), SLOT(updatePlot()) );
    connect( btnExport, SIGNAL(clicked()), SLOT(exportPlot()) );
}

void MainWindow::updatePlot()
{
    m_plot->applySettings( m_panel->settings() );
}

void MainWindow::exportPlot()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( m_plot, "legends.pdf" );
}

#include "moc_MainWindow.cpp"
