/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QMainWindow>

class Plot;
class Panel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow( QWidget* parent = 0 );

  private Q_SLOTS:
    void updatePlot();
    void exportPlot();

  private:
    Plot* m_plot;
    Panel* m_panel;
};
