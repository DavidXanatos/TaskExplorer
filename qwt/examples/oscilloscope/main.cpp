/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "MainWindow.h"
#include "SamplingThread.h"

#include <QApplication>

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );
    app.setPalette( Qt::darkGray );

    MainWindow window;
    window.resize( 800, 400 );

    SamplingThread samplingThread;
    samplingThread.setFrequency( window.frequency() );
    samplingThread.setAmplitude( window.amplitude() );
    samplingThread.setInterval( window.signalInterval() );

    window.connect( &window, SIGNAL(frequencyChanged(double)),
        &samplingThread, SLOT(setFrequency(double)) );

    window.connect( &window, SIGNAL(amplitudeChanged(double)),
        &samplingThread, SLOT(setAmplitude(double)) );

    window.connect( &window, SIGNAL(signalIntervalChanged(double)),
        &samplingThread, SLOT(setInterval(double)) );

    window.show();

    samplingThread.start();
    window.start();

    const bool ok = app.exec();

    samplingThread.stop();
    samplingThread.wait( 1000 );

    return ok;
}
