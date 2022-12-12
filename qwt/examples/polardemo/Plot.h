/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPolarPlot>

class QwtPolarGrid;
class QwtPolarCurve;

class PlotSettings
{
  public:
    enum Curve
    {
        Spiral,
        Rose,

        NumCurves
    };

    enum Flag
    {
        MajorGridBegin,
        MinorGridBegin = MajorGridBegin + QwtPolar::ScaleCount,
        AxisBegin = MinorGridBegin + QwtPolar::ScaleCount,

        AutoScaling = AxisBegin + QwtPolar::AxesCount,
        Inverted,
        Logarithmic,

        Antialiasing,

        CurveBegin,

        NumFlags = CurveBegin + NumCurves
    };

    bool flags[NumFlags];
};

class Plot : public QwtPolarPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* = NULL );
    PlotSettings settings() const;

  public Q_SLOTS:
    void applySettings( const PlotSettings& );

  private:
    QwtPolarCurve* createCurve( int id ) const;

    QwtPolarGrid* m_grid;
    QwtPolarCurve* m_curve[PlotSettings::NumCurves];
};
