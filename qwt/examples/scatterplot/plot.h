/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class QwtPlotCurve;
class QwtSymbol;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* parent = NULL );

    void setSymbol( QwtSymbol* );
    void setSamples( const QVector< QPointF >& samples );

  private:
    QwtPlotCurve* m_curve;
};
