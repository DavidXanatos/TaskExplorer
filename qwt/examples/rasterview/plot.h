/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class QwtPlotSpectrogram;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* = NULL );

  public Q_SLOTS:
    void exportPlot();
    void setResampleMode( int );

  private:
    QwtPlotSpectrogram* m_spectrogram;
};
