/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class Settings;
class QwtPlotLegendItem;
class QwtLegend;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* parent = NULL );
    virtual ~Plot();

  public Q_SLOTS:
    void applySettings( const Settings& );

  public:
    virtual void replot() QWT_OVERRIDE;

  private:
    void insertCurve();

    QwtLegend* m_externalLegend;
    QwtPlotLegendItem* m_legendItem;
    bool m_isDirty;
};
