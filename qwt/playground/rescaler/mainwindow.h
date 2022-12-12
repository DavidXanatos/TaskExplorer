/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QMainWindow>

class QwtPlotRescaler;
class QLabel;
class Plot;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    enum RescaleMode
    {
        KeepScales,
        Fixed,
        Expanding,
        Fitting
    };

    MainWindow();

  private Q_SLOTS:
    void setRescaleMode( int );
    void showRatio( double, double );

  private:
    QWidget* createPanel( QWidget* );
    Plot* createPlot( QWidget* );

    QwtPlotRescaler* m_rescaler;
    QLabel* m_rescaleInfo;

    Plot* m_plot;
};
