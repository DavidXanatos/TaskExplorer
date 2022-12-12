/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "BarChart.h"

#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>
#include <QComboBox>

namespace
{
    class MainWindow : public QMainWindow
    {
      public:
        MainWindow( QWidget* = NULL );
    };
}

MainWindow::MainWindow( QWidget* parent )
    : QMainWindow( parent )
{
    BarChart* chart = new BarChart();
    setCentralWidget( chart );

    QToolBar* toolBar = new QToolBar();

    QComboBox* typeBox = new QComboBox( toolBar );
    typeBox->addItem( "Grouped" );
    typeBox->addItem( "Stacked" );
    typeBox->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

    QComboBox* orientationBox = new QComboBox( toolBar );
    orientationBox->addItem( "Vertical" );
    orientationBox->addItem( "Horizontal" );
    orientationBox->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

    QToolButton* btnExport = new QToolButton( toolBar );
    btnExport->setText( "Export" );
    btnExport->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    connect( btnExport, SIGNAL(clicked()), chart, SLOT(exportChart()) );

    toolBar->addWidget( typeBox );
    toolBar->addWidget( orientationBox );
    toolBar->addWidget( btnExport );
    addToolBar( toolBar );

    chart->setMode( typeBox->currentIndex() );
    connect( typeBox, SIGNAL(currentIndexChanged(int)),
        chart, SLOT(setMode(int)) );

    chart->setOrientation( orientationBox->currentIndex() );
    connect( orientationBox, SIGNAL(currentIndexChanged(int)),
        chart, SLOT(setOrientation(int)) );
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    MainWindow window;

    window.resize( 600, 400 );
    window.show();

    return app.exec();
}
