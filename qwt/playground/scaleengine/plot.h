/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class QwtTransform;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* parent = NULL );

  public Q_SLOTS:
    void setTransformation( QwtTransform* );

  private:
    void populate();
};
