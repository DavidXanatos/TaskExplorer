/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QMainWindow>

class RandomPlot;
class Counter;

class QCheckBox;
class QAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT
  public:
    MainWindow();

  private Q_SLOTS:
    void showRunning( bool );
    void appendPoints( bool );
    void showElapsed( int );

  private:
    QToolBar* toolBar();
    void initWhatsThis();

  private:
    Counter* m_randomCount;
    Counter* m_timerCount;
    QCheckBox* m_symbolType;
    QAction* m_startAction;
    QAction* m_clearAction;
    RandomPlot* m_plot;
};
