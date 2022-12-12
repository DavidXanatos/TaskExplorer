/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>
#include <QComboBox>
#include <QTabWidget>

namespace
{
    class ToolButton : public QToolButton
    {
      public:
        ToolButton( const char* text )
        {
            setText( text );
            setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
        }
    };

    class PlotTab : public QMainWindow
    {
      public:
        PlotTab( bool parametric,  QWidget* parent )
            : QMainWindow( parent )
        {
            Plot* plot = new Plot( parametric, this );
            setCentralWidget( plot );

            QToolBar* toolBar = new QToolBar();

#ifndef QT_NO_PRINTER
            ToolButton* btnPrint = new ToolButton( "Print" );
            toolBar->addWidget( btnPrint );
            QObject::connect( btnPrint, SIGNAL(clicked()),
                plot, SLOT(printPlot()) );
#endif

            ToolButton* btnOverlay = new ToolButton( "Overlay" );
            btnOverlay->setCheckable( true );
            toolBar->addWidget( btnOverlay );
            QObject::connect( btnOverlay, SIGNAL(toggled(bool)),
                plot, SLOT(setOverlaying(bool)) );

            if ( parametric )
            {
                QComboBox* parameterBox = new QComboBox();

                parameterBox->addItem( "Uniform" );
                parameterBox->addItem( "Centripetral" );
                parameterBox->addItem( "Chordal" );
                parameterBox->addItem( "Manhattan" );
                toolBar->addWidget( parameterBox );
#if QT_VERSION >= 0x060000
                connect( parameterBox, SIGNAL(textActivated(const QString&)),
                    plot, SLOT(setParametric(const QString&)) );
#else
                connect( parameterBox, SIGNAL(activated(const QString&)),
                    plot, SLOT(setParametric(const QString&)) );
#endif

                parameterBox->setCurrentIndex( 2 ); // chordal
                plot->setParametric( parameterBox->currentText() );

                ToolButton* btnClosed = new ToolButton( "Closed" );
                btnClosed->setCheckable( true );
                toolBar->addWidget( btnClosed );
                QObject::connect( btnClosed, SIGNAL(toggled(bool)),
                    plot, SLOT(setClosed(bool)) );
            }

            QComboBox* boundaryBox = new QComboBox();

            boundaryBox->addItem( "Natural" );
            boundaryBox->addItem( "Linear Runout" );
            boundaryBox->addItem( "Parabolic Runout" );
            boundaryBox->addItem( "Cubic Runout" );
            boundaryBox->addItem( "Not a Knot" );

            toolBar->addWidget( boundaryBox );
#if QT_VERSION >= 0x060000
            connect( boundaryBox, SIGNAL(textActivated(const QString&)),
                plot, SLOT(setBoundaryCondition(const QString&)) );
#else
            connect( boundaryBox, SIGNAL(activated(const QString&)),
                plot, SLOT(setBoundaryCondition(const QString&)) );
#endif

            addToolBar( toolBar );
        }
    };
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );
    app.setStyle( "Windows" );

    QTabWidget tabWidget;
    tabWidget.addTab( new PlotTab( false, &tabWidget ), "Non Parametric" );
    tabWidget.addTab( new PlotTab( true, &tabWidget ), "Parametric" );

    tabWidget.resize( 800, 600 );
    tabWidget.show();

    return app.exec();
}
