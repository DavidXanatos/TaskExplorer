/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>
#include <QRect>

class QwtPlotGraphicItem;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* = NULL );

  public Q_SLOTS:

#ifndef QT_NO_FILEDIALOG
    void loadSVG();
#endif

    void loadSVG( const QString& );

  private:
    void rescale();

    QwtPlotGraphicItem* m_mapItem;
    const QRectF m_mapRect;
};
