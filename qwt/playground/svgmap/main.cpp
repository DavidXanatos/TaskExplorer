/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QApplication>
#include <QMainWindow>

#ifndef QT_NO_FILEDIALOG
#include <QToolBar>
#include <QToolButton>
#endif

namespace
{
    class MainWindow : public QMainWindow
    {
      public:
        MainWindow( const QString& fileName )
        {
            Plot* plot = new Plot( this );
            if ( !fileName.isEmpty() )
                plot->loadSVG( fileName );

            setCentralWidget( plot );

#ifndef QT_NO_FILEDIALOG

            QToolButton* btnLoad = new QToolButton();
            btnLoad->setText( "Load SVG" );
            btnLoad->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );

            QToolBar* toolBar = new QToolBar();
            toolBar->addWidget( btnLoad );
            addToolBar( toolBar );

            connect( btnLoad, SIGNAL(clicked()), plot, SLOT(loadSVG()) );
#endif
        }
    };
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    QString fileName;
    if ( argc > 1 )
    {
        fileName = argv[1];
    }
    else
    {
        // see: https://commons.wikimedia.org/wiki/File:Schlosspark_Nymphenburg.svg
        fileName = ":/svg/Schlosspark_Nymphenburg.svg";
    }

    MainWindow window( fileName );
    window.resize( 600, 600 );
    window.show();

    return app.exec();
}
