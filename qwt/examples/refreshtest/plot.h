/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include "Settings.h"

#include <QwtPlot>
#include <QElapsedTimer>

class QwtPlotGrid;
class QwtPlotCurve;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* = NULL );

  public Q_SLOTS:
    void setSettings( const Settings& );

  protected:
    virtual void timerEvent( QTimerEvent* ) QWT_OVERRIDE;

  private:
    void alignScales();

    QwtPlotGrid* m_grid;
    QwtPlotCurve* m_curve;

    QElapsedTimer m_elapsedTimer;
    double m_interval;

    int m_timerId;

    Settings m_settings;
};
