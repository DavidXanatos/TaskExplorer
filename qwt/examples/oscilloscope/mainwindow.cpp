/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "MainWindow.h"
#include "Plot.h"
#include "Knob.h"
#include "WheelBox.h"

#include <QLayout>

MainWindow::MainWindow( QWidget* parent )
    : QWidget( parent )
{
    const double intervalLength = 10.0; // seconds

    m_plot = new Plot();
    m_plot->setIntervalLength( intervalLength );

    m_amplitudeKnob = new Knob( "Amplitude", 0.0, 200.0 );
    m_amplitudeKnob->setValue( 160.0 );

    m_frequencyKnob = new Knob( "Frequency [Hz]", 0.1, 20.0 );
    m_frequencyKnob->setValue( 17.8 );

    m_intervalWheel = new WheelBox( "Displayed [s]", 1.0, 100.0, 1.0 );
    m_intervalWheel->setValue( intervalLength );

    m_timerWheel = new WheelBox( "Sample Interval [ms]", 0.0, 20.0, 0.1 );
    m_timerWheel->setValue( 10.0 );

    QVBoxLayout* vLayout1 = new QVBoxLayout();
    vLayout1->addWidget( m_intervalWheel );
    vLayout1->addWidget( m_timerWheel );
    vLayout1->addStretch( 10 );
    vLayout1->addWidget( m_amplitudeKnob );
    vLayout1->addWidget( m_frequencyKnob );

    QHBoxLayout* layout = new QHBoxLayout( this );
    layout->addWidget( m_plot, 10 );
    layout->addLayout( vLayout1 );

    connect( m_amplitudeKnob, SIGNAL(valueChanged(double)),
        SIGNAL(amplitudeChanged(double)) );

    connect( m_frequencyKnob, SIGNAL(valueChanged(double)),
        SIGNAL(frequencyChanged(double)) );

    connect( m_timerWheel, SIGNAL(valueChanged(double)),
        SIGNAL(signalIntervalChanged(double)) );

    connect( m_intervalWheel, SIGNAL(valueChanged(double)),
        m_plot, SLOT(setIntervalLength(double)) );
}

void MainWindow::start()
{
    m_plot->start();
}

double MainWindow::frequency() const
{
    return m_frequencyKnob->value();
}

double MainWindow::amplitude() const
{
    return m_amplitudeKnob->value();
}

double MainWindow::signalInterval() const
{
    return m_timerWheel->value();
}

#include "moc_MainWindow.cpp"
