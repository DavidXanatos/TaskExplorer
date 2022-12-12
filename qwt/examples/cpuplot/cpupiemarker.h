/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlotItem>

class CpuPieMarker : public QwtPlotItem
{
  public:
    CpuPieMarker();

    virtual int rtti() const QWT_OVERRIDE;

    virtual void draw( QPainter*,
        const QwtScaleMap&, const QwtScaleMap&,
        const QRectF& ) const QWT_OVERRIDE;
};
