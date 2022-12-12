/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>
#include <QStringList>

class QwtPlotBarChart;

class BarChart : public QwtPlot
{
    Q_OBJECT

  public:
    BarChart( QWidget* = NULL );

  public Q_SLOTS:
    void setOrientation( int );
    void exportChart();
    void doScreenShot();

  private:
    void populate();
    void exportPNG( int width, int height );
    void render( QPainter* painter, const QRectF& targetRect );

    QwtPlotBarChart* m_chartItem;
    QStringList m_distros;
};
