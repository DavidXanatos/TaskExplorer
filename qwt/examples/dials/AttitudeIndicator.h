/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
*****************************************************************************/

#pragma once

#include <QwtDial>

class AttitudeIndicator : public QwtDial
{
    Q_OBJECT

  public:
    AttitudeIndicator( QWidget* parent = NULL );

    double angle() const { return value(); }
    double gradient() const { return m_gradient; }

  public Q_SLOTS:
    void setGradient( double );
    void setAngle( double angle ) { setValue( angle ); }

  protected:
    virtual void keyPressEvent( QKeyEvent* ) QWT_OVERRIDE;

    virtual void drawScale( QPainter*,
        const QPointF& center, double radius ) const QWT_OVERRIDE;

    virtual void drawScaleContents( QPainter* painter,
        const QPointF& center, double radius ) const QWT_OVERRIDE;

  private:
    double m_gradient;
};
