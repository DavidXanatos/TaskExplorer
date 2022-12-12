/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class QwtPlotMultiBarChart;

class BarChart : public QwtPlot
{
    Q_OBJECT

  public:
    BarChart( QWidget* = NULL );

  public Q_SLOTS:
    void setMode( int );
    void setOrientation( int );
    void exportChart();

  private:
    void populate();

    QwtPlotMultiBarChart* m_barChartItem;
};
