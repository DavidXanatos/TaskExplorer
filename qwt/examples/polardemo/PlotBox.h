/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QMainWindow>

class Plot;
class QwtPolarPanner;
class QwtPolarMagnifier;

class PlotBox : public QWidget
{
    Q_OBJECT

  public:
    PlotBox( QWidget* parent = nullptr );

  private Q_SLOTS:
    void enableZoomMode( bool on );
    void printDocument();
    void exportDocument();

  private:
    Plot* m_plot;
    QwtPolarPanner* m_panner;
    QwtPolarMagnifier* m_zoomer;
};
