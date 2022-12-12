/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtPlot>

class RectItem;
class QwtInterval;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* parent, const QwtInterval& );
    virtual void updateLayout() QWT_OVERRIDE;

    void setRectOfInterest( const QRectF& );

  Q_SIGNALS:
    void resized( double xRatio, double yRatio );

  private:
    RectItem* m_rectItem;
};
