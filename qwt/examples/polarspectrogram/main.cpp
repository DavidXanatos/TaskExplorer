/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "PlotWindow.h"
#include "Plot.h"

#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>

namespace
{
    class ToolButton : public QToolButton
    {
      public:
        ToolButton( const QString& text, QWidget* parent = NULL )
            : QToolButton( parent )
        {
            setText( text );
            setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
        };
    };

    class MainWindow : public QMainWindow
    {
      public:
        MainWindow( QWidget* = NULL );
    };
}

MainWindow::MainWindow( QWidget* parent )
    : QMainWindow( parent )
{
    PlotWindow* plotWindow = new PlotWindow();
    setCentralWidget( plotWindow );

    ToolButton* btnGrid = new ToolButton( "Grid" );
    btnGrid->setCheckable( true );
    btnGrid->setChecked( true );

    ToolButton* btnExport = new ToolButton( "Export" );
    ToolButton* btnRotate = new ToolButton( "Rotate" );
    ToolButton* btnMirror = new ToolButton( "Mirror" );

    QToolBar* toolBar = new QToolBar();

    toolBar->addWidget( btnExport );
    toolBar->addWidget( btnGrid );
    toolBar->addWidget( btnRotate );
    toolBar->addWidget( btnMirror );

    addToolBar( toolBar );

    Plot* plot = plotWindow->plot();

    connect( btnExport, SIGNAL(clicked()), plot, SLOT(exportDocument()) );
    connect( btnGrid, SIGNAL(toggled(bool)), plot, SLOT(showGrid(bool)) );
    connect( btnRotate, SIGNAL(clicked()), plot, SLOT(rotate()) );
    connect( btnMirror, SIGNAL(clicked()), plot, SLOT(mirror()) );
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    MainWindow window;
    window.resize( 700, 600 );
    window.show();

    return app.exec();
}
