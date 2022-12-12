/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class QwtPlotCurve;
class QwtPlotMarker;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* parent );

  public Q_SLOTS:
    void setDamp( double damping );

  private:
    void showData( const double* frequency, const double* amplitude,
        const double* phase, int count );
    void showPeak( double freq, double amplitude );
    void show3dB( double freq );

    QwtPlotCurve* m_curve1;
    QwtPlotCurve* m_curve2;
    QwtPlotMarker* m_marker1;
    QwtPlotMarker* m_marker2;
};
