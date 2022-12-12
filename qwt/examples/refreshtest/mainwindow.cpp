/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "MainWindow.h"
#include "Plot.h"
#include "Panel.h"

#include <QStatusBar>
#include <QLabel>
#include <QLayout>
#include <QEvent>
#include <QElapsedTimer>

MainWindow::MainWindow( QWidget* parent )
    : QMainWindow( parent )
{
    Panel* panel = new Panel();
    m_plot = new Plot();

    QWidget* w = new QWidget( this );

    QHBoxLayout* hLayout = new QHBoxLayout( w );
    hLayout->addWidget( panel );
    hLayout->addWidget( m_plot, 10 );

    setCentralWidget( w );

    m_frameCount = new QLabel( this );
    statusBar()->addWidget( m_frameCount, 10 );

    applySettings( panel->settings() );

    connect( panel, SIGNAL(settingsChanged(const Settings&)),
        this, SLOT(applySettings(const Settings&)) );
}

bool MainWindow::eventFilter( QObject* object, QEvent* event )
{
    if ( object == m_plot->canvas() && event->type() == QEvent::Paint )
    {
        static int counter;
        static QElapsedTimer timeStamp;

        if ( !timeStamp.isValid() )
        {
            timeStamp.start();
            counter = 0;
        }
        else
        {
            counter++;

            const double elapsed = timeStamp.elapsed() / 1000.0;
            if ( elapsed >= 1 )
            {
                QString fps;
                fps.setNum( qRound( counter / elapsed ) );
                fps += " Fps";

                m_frameCount->setText( fps );

                counter = 0;
                timeStamp.start();
            }
        }
    }

    return QMainWindow::eventFilter( object, event );
}

void MainWindow::applySettings( const Settings& settings )
{
    m_plot->setSettings( settings );

    // the canvas might have been recreated
    m_plot->canvas()->removeEventFilter( this );
    m_plot->canvas()->installEventFilter( this );
}

#include "moc_MainWindow.cpp"
