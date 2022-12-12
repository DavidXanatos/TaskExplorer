/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Canvas.h"
#include <QwtGraphic>
#include <QSvgRenderer>

Canvas::Canvas( Mode mode, QWidget* parent )
    : QWidget( parent )
    , m_mode( mode )
{
    const int m = 10;
    setContentsMargins( m, m, m, m );

    if ( m_mode == Svg )
        m_renderer = new QSvgRenderer( this );
    else
        m_graphic = new QwtGraphic();
}

Canvas::~Canvas()
{
    if ( m_mode == VectorGraphic )
        delete m_graphic;
}

void Canvas::setSvg( const QByteArray& svgData )
{
    if ( m_mode == VectorGraphic )
    {
        m_graphic->reset();

        QSvgRenderer renderer;
        renderer.load( svgData );

        QPainter p( m_graphic );
        renderer.render( &p, renderer.viewBoxF() );
        p.end();
    }
    else
    {
        m_renderer->load( svgData );
    }

    update();
}

void Canvas::paintEvent( QPaintEvent* )
{
    QPainter painter( this );

    painter.save();

    painter.setPen( Qt::black );
    painter.setBrush( Qt::white );
    painter.drawRect( contentsRect().adjusted( 0, 0, -1, -1 ) );

    painter.restore();

    painter.setPen( Qt::NoPen );
    painter.setBrush( Qt::NoBrush );
    render( &painter, contentsRect() );
}

void Canvas::render( QPainter* painter, const QRect& rect ) const
{
    if ( m_mode == Svg )
    {
        m_renderer->render( painter, rect );
    }
    else
    {
        m_graphic->render( painter, rect );
    }
}
