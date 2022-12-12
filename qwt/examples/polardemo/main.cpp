/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "PlotBox.h"
#include "Pixmaps.h"

#include <QApplication>
#include <QToolBar>
#include <QToolButton>

namespace
{
    class MainWindow : public QMainWindow
    {
      public:
        MainWindow()
        {
            PlotBox* plotBox = new PlotBox( this );
            setCentralWidget( plotBox );

            QToolButton* btnZoom = new QToolButton();

            const QString zoomHelp =
                "Use the wheel to zoom in/out.\n"
                "When the plot is zoomed in,\n"
                "use the left mouse button to move it.";

            btnZoom->setText( "Zoom" );
            btnZoom->setIcon( QPixmap( zoom_xpm ) );
            btnZoom->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
            btnZoom->setToolTip( zoomHelp );
            btnZoom->setCheckable( true );
            connect( btnZoom, SIGNAL(toggled(bool)),
                plotBox, SLOT(enableZoomMode(bool)) );

            QToolButton* btnPrint = new QToolButton();
            btnPrint->setText( "Print" );
            btnPrint->setIcon( QPixmap( print_xpm ) );
            btnPrint->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
            connect( btnPrint, SIGNAL(clicked()),
                plotBox, SLOT(printDocument()) );

            QToolButton* btnExport = new QToolButton();
            btnExport->setText( "Export" );
            btnExport->setIcon( QPixmap( print_xpm ) );
            btnExport->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );

            QToolBar* toolBar = new QToolBar();
            toolBar->addWidget( btnZoom );
            toolBar->addWidget( btnPrint );
            toolBar->addWidget( btnExport );

            connect( btnExport, SIGNAL(clicked()),
                plotBox, SLOT(exportDocument()) );

            addToolBar( toolBar );
        }
    };
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    MainWindow window;
    window.resize( 800, 600 );
    window.show();

    return app.exec();
}

