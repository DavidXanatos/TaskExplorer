/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class QwtPlotCurve;
class QwtPlotDirectPainter;

class IncrementalPlot : public QwtPlot
{
    Q_OBJECT

  public:
    IncrementalPlot( QWidget* parent = NULL );
    virtual ~IncrementalPlot();

    void appendPoint( const QPointF& );
    void clearPoints();

  public Q_SLOTS:
    void showSymbols( bool );

  private:
    QwtPlotCurve* m_curve;
    QwtPlotDirectPainter* m_directPainter;
};
