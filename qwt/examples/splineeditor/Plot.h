/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class QwtWheel;
class QwtPlotMarker;
class QwtPlotCurve;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( bool parametric, QWidget* parent = NULL );

    virtual bool eventFilter( QObject*, QEvent* ) QWT_OVERRIDE;

  public Q_SLOTS:
    void updateMarker( int axis, double base );
    void legendChecked( const QVariant&, bool on );
    void setOverlaying( bool );
    void setParametric( const QString& );
    void setBoundaryCondition( const QString& );
    void setClosed( bool );

#ifndef QT_NO_PRINTER
    void printPlot();
#endif

  private Q_SLOTS:
    void scrollLeftAxis( double );

  private:
    void showCurve( QwtPlotItem*, bool on );

    QwtPlotMarker* m_marker;
    QwtPlotCurve* m_curve;
    QwtWheel* m_wheel;

    int m_boundaryCondition;
};
