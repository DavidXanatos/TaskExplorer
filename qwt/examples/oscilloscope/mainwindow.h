/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QWidget>

class Plot;
class Knob;
class WheelBox;

class MainWindow : public QWidget
{
    Q_OBJECT

  public:
    MainWindow( QWidget* = NULL );

    void start();

    double amplitude() const;
    double frequency() const;
    double signalInterval() const;

  Q_SIGNALS:
    void amplitudeChanged( double );
    void frequencyChanged( double );
    void signalIntervalChanged( double );

  private:
    Knob* m_frequencyKnob;
    Knob* m_amplitudeKnob;
    WheelBox* m_timerWheel;
    WheelBox* m_intervalWheel;

    Plot* m_plot;
};
