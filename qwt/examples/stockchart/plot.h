/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* = NULL );

  public Q_SLOTS:
    void setMode( int );
    void exportPlot();

  private Q_SLOTS:
    void showItem( QwtPlotItem*, bool on );

  private:
    void populate();
};
