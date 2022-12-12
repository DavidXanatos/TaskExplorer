/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtGlobal>
#include <QMainWindow>

class Plot;
class QLabel;
class Settings;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow( QWidget* parent = NULL );
    virtual bool eventFilter( QObject*, QEvent* ) QWT_OVERRIDE;

  private Q_SLOTS:
    void applySettings( const Settings& );

  private:
    Plot* m_plot;
    QLabel* m_frameCount;
};
