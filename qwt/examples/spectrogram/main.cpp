/*****************************************************************************
* Qwt Examples - Copyright (C) 2002 Uwe Rathmann
* This file may be used under the terms of the 3-clause BSD License
*****************************************************************************/

#include "Plot.h"

#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QStatusBar>
#include <QToolButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>

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
    Plot* plot = new Plot();
    plot->setContentsMargins( 0, 5, 0, 10 );

    setCentralWidget( plot );

#ifndef QT_NO_PRINTER
    QToolButton* btnPrint = new QToolButton();
    btnPrint->setText( "Print" );
    btnPrint->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    connect( btnPrint, SIGNAL(clicked()), plot, SLOT(printPlot()) );
#endif

    QComboBox* mapBox = new QComboBox();
    mapBox->addItem( "RGB" );
    mapBox->addItem( "Hue" );
    mapBox->addItem( "Saturation" );
    mapBox->addItem( "Value" );
    mapBox->addItem( "Sat.+Value" );
    mapBox->addItem( "Alpha" );
    mapBox->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
    connect( mapBox, SIGNAL(currentIndexChanged(int)),
        plot, SLOT(setColorMap(int)) );


    QComboBox* colorTableBox = new QComboBox();
    colorTableBox->addItem( "None" );
    colorTableBox->addItem( "256" );
    colorTableBox->addItem( "1024" );
    colorTableBox->addItem( "16384" );
    connect( colorTableBox, SIGNAL(currentIndexChanged(int)),
        plot, SLOT(setColorTableSize(int)) );

    QSlider* slider = new QSlider( Qt::Horizontal );
    slider->setRange( 0, 255 );
    slider->setValue( 255 );
    connect( slider, SIGNAL(valueChanged(int)), plot, SLOT(setAlpha(int)) );

    QCheckBox* btnSpectrogram = new QCheckBox( "Spectrogram" );
    btnSpectrogram->setChecked( true );
    connect( btnSpectrogram, SIGNAL(toggled(bool)),
        plot, SLOT(showSpectrogram(bool)) );

    QCheckBox* btnContour = new QCheckBox( "Contour" );
    btnContour->setChecked( false );
    connect( btnContour, SIGNAL(toggled(bool)),
        plot, SLOT(showContour(bool)) );

    QToolBar* toolBar = new QToolBar();

#ifndef QT_NO_PRINTER
    toolBar->addWidget( btnPrint );
    toolBar->addSeparator();
#endif
    toolBar->addWidget( new QLabel("Color Map " ) );
    toolBar->addWidget( mapBox );

    toolBar->addWidget( new QLabel("Table " ) );
    toolBar->addWidget( colorTableBox );

    toolBar->addWidget( new QLabel( " Opacity " ) );
    toolBar->addWidget( slider );

    toolBar->addWidget( new QLabel("   " ) );
    toolBar->addWidget( btnSpectrogram );
    toolBar->addWidget( btnContour );

    addToolBar( toolBar );

    connect( plot, SIGNAL(rendered(const QString&)),
        statusBar(), SLOT(showMessage(const QString&)),
        Qt::QueuedConnection );
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );
    app.setStyle( "Windows" );

    MainWindow window;
    window.resize( 600, 400 );
    window.show();

    return app.exec();
}
