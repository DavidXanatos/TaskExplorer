/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtGlobal>
#include <QWidget>

class QSvgRenderer;
class QwtGraphic;

class Canvas : public QWidget
{
  public:
    enum Mode
    {
        Svg,
        VectorGraphic
    };

    Canvas( Mode, QWidget* parent = NULL );
    virtual ~Canvas();

    void setSvg( const QByteArray& );

  protected:
    virtual void paintEvent( QPaintEvent* ) QWT_OVERRIDE;

  private:
    void render( QPainter*, const QRect& ) const;

    const Mode m_mode;
    union
    {
        QSvgRenderer* m_renderer;
        QwtGraphic* m_graphic;
    };
};
