/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"

#include <QwtPlotGrid>
#include <QwtPlotItem>
#include <QwtScaleMap>
#include <QwtPlotLayout>
#include <QwtInterval>
#include <QwtPainter>
#include <QwtMath>

#include <QPainter>

class TextItem : public QwtPlotItem
{
  public:
    void setText( const QString& text )
    {
        m_text = text;
    }

    virtual void draw( QPainter* painter,
        const QwtScaleMap&, const QwtScaleMap&,
        const QRectF& canvasRect ) const QWT_OVERRIDE
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

class RectItem : public QwtPlotItem
{
  public:
    enum Type
    {
        Rect,
        Ellipse
    };

    explicit RectItem( Type type )
        : m_type( type )
    {
    }

    void setPen( const QPen& pen )
    {
        if ( pen != m_pen )
        {
            m_pen = pen;
            itemChanged();
        }
    }

    void setBrush( const QBrush& brush )
    {
        if ( brush != m_brush )
        {
            m_brush = brush;
            itemChanged();
        }
    }
    void setRect( const QRectF& rect )
    {
        if ( m_rect != rect )
        {
            m_rect = rect;
            itemChanged();
        }
    }

    virtual QRectF boundingRect() const QWT_OVERRIDE
    {
        return m_rect;
    }

    virtual void draw( QPainter* painter,
        const QwtScaleMap& xMap, const QwtScaleMap& yMap,
        const QRectF& ) const QWT_OVERRIDE
    {
        if ( m_rect.isValid() )
        {
            const QRectF rect = QwtScaleMap::transform(
                xMap, yMap, m_rect );

            painter->setPen( m_pen );
            painter->setBrush( m_brush );

            if ( m_type == Ellipse )
                QwtPainter::drawEllipse( painter, rect );
            else
                QwtPainter::drawRect( painter, rect );
        }
    }

  private:
    QPen m_pen;
    QBrush m_brush;
    QRectF m_rect;
    Type m_type;
};

Plot::Plot( QWidget* parent, const QwtInterval& interval )
    : QwtPlot( parent )
{
    for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
        setAxisScale( axisPos, interval.minValue(), interval.maxValue() );

    setCanvasBackground( QColor( Qt::darkBlue ) );
    plotLayout()->setAlignCanvasToScales( true );

    // grid
    QwtPlotGrid* grid = new QwtPlotGrid;
    //grid->enableXMin(true);
    grid->setMajorPen( Qt::white, 0, Qt::DotLine );
    grid->setMinorPen( Qt::gray, 0, Qt::DotLine );
    grid->attach( this );

    const int numEllipses = 10;

    for ( int i = 0; i < numEllipses; i++ )
    {
        const double x = interval.minValue() +
            qwtRand() % qRound( interval.width() );
        const double y = interval.minValue() +
            qwtRand() % qRound( interval.width() );
        const double r = interval.minValue() +
            qwtRand() % qRound( interval.width() / 6 );

        const QRectF area( x - r, y - r, 2 * r, 2 * r );

        RectItem* item = new RectItem( RectItem::Ellipse );
        item->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        item->setRect( area );
        item->setPen( QPen( Qt::yellow ) );
        item->attach( this );
    }

    TextItem* textItem = new TextItem();
    textItem->setText( "Navigation Example" );
    textItem->attach( this );

    m_rectItem = new RectItem( RectItem::Rect );
    m_rectItem->setPen( Qt::NoPen );
    QColor c = Qt::gray;
    c.setAlpha( 100 );
    m_rectItem->setBrush( QBrush( c ) );
    m_rectItem->attach( this );
}

void Plot::updateLayout()
{
    QwtPlot::updateLayout();

    const QwtScaleMap xMap = canvasMap( QwtAxis::XBottom );
    const QwtScaleMap yMap = canvasMap( QwtAxis::YLeft );

    const QRect cr = canvas()->contentsRect();
    const double x1 = xMap.invTransform( cr.left() );
    const double x2 = xMap.invTransform( cr.right() );
    const double y1 = yMap.invTransform( cr.bottom() );
    const double y2 = yMap.invTransform( cr.top() );

    const double xRatio = ( x2 - x1 ) / cr.width();
    const double yRatio = ( y2 - y1 ) / cr.height();

    Q_EMIT resized( xRatio, yRatio );
}

void Plot::setRectOfInterest( const QRectF& rect )
{
    m_rectItem->setRect( rect );
}

#include "moc_Plot.cpp"
