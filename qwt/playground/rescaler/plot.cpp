#include "plot.h"
#include <qglobal.h>
#include <qpainter.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_layout.h>
#include <qwt_interval.h>
#include <qwt_painter.h>
#include <qwt_plot_item.h>

class TextItem: public QwtPlotItem
{
public:
    void setText( const QString &text )
    {
        m_text = text;
    }

    virtual void draw( QPainter *painter,
        const QwtScaleMap &, const QwtScaleMap &,
        const QRectF &canvasRect ) const
    {
        const int margin = 5;
        const QRectF textRect = 
            canvasRect.adjusted( margin, margin, -margin, -margin );

        painter->setPen( Qt::white );
        painter->drawText( textRect, 
            Qt::AlignBottom | Qt::AlignRight, m_text );
    }

private:
    QString m_text;
};


// RectItem shows how to implement a simple plot item,
// what wouldn't be necessary as QwtPlotShapeItem
// would do the same
class RectItem: public QwtPlotItem
{
public:
    enum Type
    {
        Rect,
        Ellipse
    };

    RectItem( Type type ):
        d_type( type )
    {
    }

    void setPen( const QPen &pen )
    {
        if ( pen != d_pen )
        {
            d_pen = pen;
            itemChanged();
        }
    }

    void setBrush( const QBrush &brush )
    {
        if ( brush != d_brush )
        {
            d_brush = brush;
            itemChanged();
        }
    }
    void setRect( const QRectF &rect )
    {
        if ( d_rect != rect )
        {
            d_rect = rect;
            itemChanged();
        }
    }

    virtual QRectF boundingRect() const
    {
        return d_rect;
    }

    virtual void draw( QPainter *painter,
        const QwtScaleMap &xMap, const QwtScaleMap &yMap,
        const QRectF & ) const
    {
        if ( d_rect.isValid() )
        {
            const QRectF rect = QwtScaleMap::transform(
                xMap, yMap, d_rect );

            painter->setPen( d_pen );
            painter->setBrush( d_brush );

            if ( d_type == Ellipse )
                QwtPainter::drawEllipse( painter, rect );
            else
                QwtPainter::drawRect( painter, rect );
        }
    }

private:
    QPen d_pen;
    QBrush d_brush;
    QRectF d_rect;
    Type d_type;
};

Plot::Plot( QWidget *parent, const QwtInterval &interval ):
    QwtPlot( parent )
{
    for ( int axis = 0; axis < QwtPlot::axisCnt; axis ++ )
        setAxisScale( axis, interval.minValue(), interval.maxValue() );

    setCanvasBackground( QColor( Qt::darkBlue ) );
    plotLayout()->setAlignCanvasToScales( true );

    // grid
    QwtPlotGrid *grid = new QwtPlotGrid;
    //grid->enableXMin(true);
    grid->setMajorPen( Qt::white, 0, Qt::DotLine );
    grid->setMinorPen( Qt::gray, 0 , Qt::DotLine );
    grid->attach( this );

    const int numEllipses = 10;

    for ( int i = 0; i < numEllipses; i++ )
    {
        const double x = interval.minValue() +
            qrand() % qRound( interval.width() );
        const double y = interval.minValue() +
            qrand() % qRound( interval.width() );
        const double r = interval.minValue() +
            qrand() % qRound( interval.width() / 6 );

        const QRectF area( x - r, y - r , 2 * r, 2 * r );

        RectItem *item = new RectItem( RectItem::Ellipse );
        item->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        item->setRect( area );
        item->setPen( QPen( Qt::yellow ) );
        item->attach( this );
    }

    TextItem *textItem = new TextItem();
    textItem->setText( "Navigation Example" );
    textItem->attach( this );

    d_rectOfInterest = new RectItem( RectItem::Rect );
    d_rectOfInterest->setPen( Qt::NoPen );
    QColor c = Qt::gray;
    c.setAlpha( 100 );
    d_rectOfInterest->setBrush( QBrush( c ) );
    d_rectOfInterest->attach( this );
}

void Plot::updateLayout()
{
    QwtPlot::updateLayout();

    const QwtScaleMap xMap = canvasMap( QwtPlot::xBottom );
    const QwtScaleMap yMap = canvasMap( QwtPlot::yLeft );

    const QRect cr = canvas()->contentsRect();
    const double x1 = xMap.invTransform( cr.left() );
    const double x2 = xMap.invTransform( cr.right() );
    const double y1 = yMap.invTransform( cr.bottom() );
    const double y2 = yMap.invTransform( cr.top() );

    const double xRatio = ( x2 - x1 ) / cr.width();
    const double yRatio = ( y2 - y1 ) / cr.height();

    Q_EMIT resized( xRatio, yRatio );
}

void Plot::setRectOfInterest( const QRectF &rect )
{
    d_rectOfInterest->setRect( rect );
}
