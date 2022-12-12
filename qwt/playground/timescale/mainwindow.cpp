/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "MainWindow.h"
#include "Plot.h"
#include "Panel.h"

#include <QwtDate>
#include <QwtScaleWidget>

#include <QLayout>

MainWindow::MainWindow( QWidget* parent )
    : QMainWindow( parent )
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

    m_plot = new Plot();
    m_panel = new Panel();
    m_panel->setSettings( settings );

    QWidget* box = new QWidget( this );

    QHBoxLayout* layout = new QHBoxLayout( box );
    layout->addWidget( m_plot, 10 );
    layout->addWidget( m_panel );

    setCentralWidget( box );

    updatePlot();

    connect( m_panel, SIGNAL(edited()), SLOT(updatePlot()) );
    connect( m_plot->axisWidget( QwtAxis::YLeft ),
        SIGNAL(scaleDivChanged()), SLOT(updatePanel()) );
}

void MainWindow::updatePlot()
{
    m_plot->blockSignals( true );
    m_plot->applySettings( m_panel->settings() );
    m_plot->blockSignals( false );
}

void MainWindow::updatePanel()
{
    const QwtScaleDiv scaleDiv = m_plot->axisScaleDiv( QwtAxis::YLeft );

    Settings settings = m_panel->settings();
    settings.startDateTime = QwtDate::toDateTime( scaleDiv.lowerBound(), Qt::LocalTime );
    settings.endDateTime = QwtDate::toDateTime( scaleDiv.upperBound(), Qt::LocalTime );

    m_panel->setSettings( settings );
}

#include "moc_MainWindow.cpp"
