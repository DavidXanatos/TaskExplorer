#include "canvas.h"
#include <qwt_graphic.h>
#include <qsvgrenderer.h>

Canvas::Canvas( Mode mode, QWidget *parent ):
    QWidget( parent ),
    d_mode( mode )
{
    const int m = 10;
    setContentsMargins( m, m, m, m );

    if ( d_mode == Svg )
        d_renderer = new QSvgRenderer( this );
    else
        d_graphic = new QwtGraphic();
}

Canvas::~Canvas()
{
    if ( d_mode == VectorGraphic )
        delete d_graphic;
}

void Canvas::setSvg( const QByteArray &data )
{
    if ( d_mode == VectorGraphic )
    {
        d_graphic->reset();

        QSvgRenderer renderer;
        renderer.load( data );

        QPainter p( d_graphic );
        renderer.render( &p, renderer.viewBoxF() );
        p.end();
    }
    else
    {
        d_renderer->load( data );
    }

    update();
}

void Canvas::paintEvent( QPaintEvent * )
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

void Canvas::render( QPainter *painter, const QRect &rect ) const
{
    if ( d_mode == Svg )
    {
        d_renderer->render( painter, rect );
    }
    else
    {
        d_graphic->render( painter, rect );
    }
}
