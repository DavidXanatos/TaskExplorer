/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QMainWindow>

class Plot;
class TransformPlot;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow( QWidget* parent = 0 );

  private:
    Plot* m_plot;
    TransformPlot* m_transformPlot;
};
