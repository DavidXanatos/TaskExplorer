/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>
#include <QwtScaleDiv>

class QwtPlotCurve;
class QwtPlotIntervalCurve;
class QwtIntervalSample;

#if QT_VERSION < 0x060000
template< typename T > class QVector;
#endif

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    enum Mode
    {
        Bars,
        Tube
    };

    Plot( QWidget* = NULL );

  public Q_SLOTS:
    void setMode( int );
    void exportPlot();

  private:
    void insertCurve( const QString& title,
        const QVector< QPointF >&, const QColor& );

    void insertErrorBars( const QString& title,
        const QVector< QwtIntervalSample >&,
        const QColor& color );


    QwtScaleDiv yearScaleDiv() const;

    QwtPlotIntervalCurve* m_intervalCurve;
    QwtPlotCurve* m_curve;
};
