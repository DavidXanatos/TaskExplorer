/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class Settings;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* parent = NULL );

  public Q_SLOTS:
    void applySettings( const Settings& );

  private:
    void initAxis( int axis, const QString& title, Qt::TimeSpec );
    void applyAxisSettings( int axis, const Settings& );
};
