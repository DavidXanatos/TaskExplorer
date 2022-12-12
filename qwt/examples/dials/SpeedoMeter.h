/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QString>
#include <QwtDial>

class SpeedoMeter : public QwtDial
{
  public:
    SpeedoMeter( QWidget* parent = NULL );

    void setLabel( const QString& );
    QString label() const;

  protected:
    virtual void drawScaleContents( QPainter* painter,
        const QPointF& center, double radius ) const QWT_OVERRIDE;

  private:
    QString m_label;
};
