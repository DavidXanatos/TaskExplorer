/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include "CpuStat.h"
#include <QwtPlot>

#define HISTORY 60 // seconds

class QwtPlotCurve;

class CpuPlot : public QwtPlot
{
    Q_OBJECT
  public:
    enum CpuData
    {
        User,
        System,
        Total,
        Idle,

        NCpuData
    };

    CpuPlot( QWidget* = 0 );
    const QwtPlotCurve* cpuCurve( int id ) const
    {
        return m_data[id].curve;
    }

  protected:
    void timerEvent( QTimerEvent* ) QWT_OVERRIDE;

  private Q_SLOTS:
    void legendChecked( const QVariant&, bool on );

  private:
    void showCurve( QwtPlotItem*, bool on );

    struct
    {
        QwtPlotCurve* curve;
        double data[HISTORY];
    } m_data[NCpuData];

    double m_timeData[HISTORY];

    int m_dataCount;
    CpuStat m_cpuStat;
};
