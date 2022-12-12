/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "ScrollZoomer.h"
#include "ScrollBar.h"

#include <QwtPlotLayout>
#include <QwtScaleWidget>

#include <QResizeEvent>
#include <QMargins>

class ScrollData
{
  public:
    ScrollData()
        : scrollBar( NULL )
        , position( ScrollZoomer::OppositeToScale )
        , mode( Qt::ScrollBarAsNeeded )
    {
    }

    ~ScrollData()
    {
        delete scrollBar;
    }

    ScrollBar* scrollBar;
    ScrollZoomer::ScrollBarPosition position;
    Qt::ScrollBarPolicy mode;
};

ScrollZoomer::ScrollZoomer( QWidget* canvas )
    : QwtPlotZoomer( canvas )
    , m_cornerWidget( NULL )
    , m_hScrollData( NULL )
    , m_vScrollData( NULL )
    , m_inZoom( false )
{
    for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
        m_alignCanvasToScales[ axisPos ] = false;

    if ( !canvas )
        return;

    m_hScrollData = new ScrollData;
    m_vScrollData = new ScrollData;
}

ScrollZoomer::~ScrollZoomer()
{
    delete m_cornerWidget;
    delete m_vScrollData;
    delete m_hScrollData;
}

void ScrollZoomer::rescale()
{
    QwtScaleWidget* xScale = plot()->axisWidget( xAxis() );
    QwtScaleWidget* yScale = plot()->axisWidget( yAxis() );

    if ( zoomRectIndex() <= 0 )
    {
        if ( m_inZoom )
        {
            xScale->setMinBorderDist( 0, 0 );
            yScale->setMinBorderDist( 0, 0 );

            QwtPlotLayout* layout = plot()->plotLayout();

            for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
                layout->setAlignCanvasToScale( axisPos, m_alignCanvasToScales[ axisPos ] );

            m_inZoom = false;
        }
    }
    else
    {
        if ( !m_inZoom )
        {
            /*
               We set a minimum border distance.
               Otherwise the canvas size changes when scrolling,
               between situations where the major ticks are at
               the canvas borders (requiring extra space for the label)
               and situations where all labels can be painted below/top
               or left/right of the canvas.
             */
            int start, end;

            xScale->getBorderDistHint( start, end );
            xScale->setMinBorderDist( start, end );

            yScale->getBorderDistHint( start, end );
            yScale->setMinBorderDist( start, end );

            QwtPlotLayout* layout = plot()->plotLayout();
            for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
            {
                m_alignCanvasToScales[axisPos] =
                    layout->alignCanvasToScale( axisPos );
            }

            layout->setAlignCanvasToScales( false );

            m_inZoom = true;
        }
    }

    QwtPlotZoomer::rescale();
    updateScrollBars();
}

ScrollBar* ScrollZoomer::scrollBar( Qt::Orientation orientation )
{
    ScrollBar*& sb = ( orientation == Qt::Vertical )
        ? m_vScrollData->scrollBar : m_hScrollData->scrollBar;

    if ( sb == NULL )
    {
        sb = new ScrollBar( orientation, canvas() );
        sb->hide();
        connect( sb,
            SIGNAL(valueChanged(Qt::Orientation,double,double)),
            SLOT(scrollBarMoved(Qt::Orientation,double,double)) );
    }
    return sb;
}

ScrollBar* ScrollZoomer::horizontalScrollBar() const
{
    return m_hScrollData->scrollBar;
}

ScrollBar* ScrollZoomer::verticalScrollBar() const
{
    return m_vScrollData->scrollBar;
}

void ScrollZoomer::setHScrollBarMode( Qt::ScrollBarPolicy mode )
{
    if ( hScrollBarMode() != mode )
    {
        m_hScrollData->mode = mode;
        updateScrollBars();
    }
}

void ScrollZoomer::setVScrollBarMode( Qt::ScrollBarPolicy mode )
{
    if ( vScrollBarMode() != mode )
    {
        m_vScrollData->mode = mode;
        updateScrollBars();
    }
}

Qt::ScrollBarPolicy ScrollZoomer::hScrollBarMode() const
{
    return m_hScrollData->mode;
}

Qt::ScrollBarPolicy ScrollZoomer::vScrollBarMode() const
{
    return m_vScrollData->mode;
}

void ScrollZoomer::setHScrollBarPosition( ScrollBarPosition pos )
{
    if ( m_hScrollData->position != pos )
    {
        m_hScrollData->position = pos;
        updateScrollBars();
    }
}

void ScrollZoomer::setVScrollBarPosition( ScrollBarPosition pos )
{
    if ( m_vScrollData->position != pos )
    {
        m_vScrollData->position = pos;
        updateScrollBars();
    }
}

ScrollZoomer::ScrollBarPosition ScrollZoomer::hScrollBarPosition() const
{
    return m_hScrollData->position;
}

ScrollZoomer::ScrollBarPosition ScrollZoomer::vScrollBarPosition() const
{
    return m_vScrollData->position;
}

void ScrollZoomer::setCornerWidget( QWidget* w )
{
    if ( w != m_cornerWidget )
    {
        if ( canvas() )
        {
            delete m_cornerWidget;
            m_cornerWidget = w;
            if ( m_cornerWidget->parent() != canvas() )
                m_cornerWidget->setParent( canvas() );

            updateScrollBars();
        }
    }
}

QWidget* ScrollZoomer::cornerWidget() const
{
    return m_cornerWidget;
}

bool ScrollZoomer::eventFilter( QObject* object, QEvent* event )
{
    if ( object == canvas() )
    {
        switch( event->type() )
        {
            case QEvent::Resize:
            {
                const QMargins m = canvas()->contentsMargins();

                QRect rect;
                rect.setSize( static_cast< QResizeEvent* >( event )->size() );
                rect.adjust( m.left(), m.top(), -m.right(), -m.bottom() );

                layoutScrollBars( rect );
                break;
            }
            case QEvent::ChildRemoved:
            {
                const QObject* child =
                    static_cast< QChildEvent* >( event )->child();

                if ( child == m_cornerWidget )
                {
                    m_cornerWidget = NULL;
                }
                else if ( child == m_hScrollData->scrollBar )
                {
                    m_hScrollData->scrollBar = NULL;
                }
                else if ( child == m_vScrollData->scrollBar )
                {
                    m_vScrollData->scrollBar = NULL;
                }
                break;
            }
            default:
                break;
        }
    }
    return QwtPlotZoomer::eventFilter( object, event );
}

bool ScrollZoomer::needScrollBar( Qt::Orientation orientation ) const
{
    Qt::ScrollBarPolicy mode;
    double zoomMin, zoomMax, baseMin, baseMax;

    if ( orientation == Qt::Horizontal )
    {
        mode = m_hScrollData->mode;
        baseMin = zoomBase().left();
        baseMax = zoomBase().right();
        zoomMin = zoomRect().left();
        zoomMax = zoomRect().right();
    }
    else
    {
        mode = m_vScrollData->mode;
        baseMin = zoomBase().top();
        baseMax = zoomBase().bottom();
        zoomMin = zoomRect().top();
        zoomMax = zoomRect().bottom();
    }

    bool needed = false;
    switch( mode )
    {
        case Qt::ScrollBarAlwaysOn:
            needed = true;
            break;

        case Qt::ScrollBarAlwaysOff:
            needed = false;
            break;

        default:
        {
            if ( baseMin < zoomMin || baseMax > zoomMax )
                needed = true;
            break;
        }
    }
    return needed;
}

void ScrollZoomer::updateScrollBars()
{
    if ( !canvas() )
        return;

    const int xAxis = QwtPlotZoomer::xAxis();
    const int yAxis = QwtPlotZoomer::yAxis();

    int xScrollBarAxis = xAxis;
    if ( hScrollBarPosition() == OppositeToScale )
        xScrollBarAxis = oppositeAxis( xScrollBarAxis );

    int yScrollBarAxis = yAxis;
    if ( vScrollBarPosition() == OppositeToScale )
        yScrollBarAxis = oppositeAxis( yScrollBarAxis );


    QwtPlotLayout* layout = plot()->plotLayout();

    bool showHScrollBar = needScrollBar( Qt::Horizontal );
    if ( showHScrollBar )
    {
        ScrollBar* sb = scrollBar( Qt::Horizontal );
        sb->setPalette( plot()->palette() );
        sb->setInverted( !plot()->axisScaleDiv( xAxis ).isIncreasing() );
        sb->setBase( zoomBase().left(), zoomBase().right() );
        sb->moveSlider( zoomRect().left(), zoomRect().right() );

        if ( !sb->isVisibleTo( canvas() ) )
        {
            sb->show();
            layout->setCanvasMargin( layout->canvasMargin( xScrollBarAxis )
                + sb->extent(), xScrollBarAxis );
        }
    }
    else
    {
        if ( horizontalScrollBar() )
        {
            horizontalScrollBar()->hide();
            layout->setCanvasMargin( layout->canvasMargin( xScrollBarAxis )
                - horizontalScrollBar()->extent(), xScrollBarAxis );
        }
    }

    bool showVScrollBar = needScrollBar( Qt::Vertical );
    if ( showVScrollBar )
    {
        ScrollBar* sb = scrollBar( Qt::Vertical );
        sb->setPalette( plot()->palette() );
        sb->setInverted( !plot()->axisScaleDiv( yAxis ).isIncreasing() );
        sb->setBase( zoomBase().top(), zoomBase().bottom() );
        sb->moveSlider( zoomRect().top(), zoomRect().bottom() );

        if ( !sb->isVisibleTo( canvas() ) )
        {
            sb->show();
            layout->setCanvasMargin( layout->canvasMargin( yScrollBarAxis )
                + sb->extent(), yScrollBarAxis );
        }
    }
    else
    {
        if ( verticalScrollBar() )
        {
            verticalScrollBar()->hide();
            layout->setCanvasMargin( layout->canvasMargin( yScrollBarAxis )
                - verticalScrollBar()->extent(), yScrollBarAxis );
        }
    }

    if ( showHScrollBar && showVScrollBar )
    {
        if ( m_cornerWidget == NULL )
        {
            m_cornerWidget = new QWidget( canvas() );
            m_cornerWidget->setAutoFillBackground( true );
            m_cornerWidget->setPalette( plot()->palette() );
        }
        m_cornerWidget->show();
    }
    else
    {
        if ( m_cornerWidget )
            m_cornerWidget->hide();
    }

    layoutScrollBars( canvas()->contentsRect() );
    plot()->updateLayout();
}

void ScrollZoomer::layoutScrollBars( const QRect& rect )
{
    int hPos = xAxis();
    if ( hScrollBarPosition() == OppositeToScale )
        hPos = oppositeAxis( hPos );

    int vPos = yAxis();
    if ( vScrollBarPosition() == OppositeToScale )
        vPos = oppositeAxis( vPos );

    ScrollBar* hScrollBar = horizontalScrollBar();
    ScrollBar* vScrollBar = verticalScrollBar();

    const int hdim = hScrollBar ? hScrollBar->extent() : 0;
    const int vdim = vScrollBar ? vScrollBar->extent() : 0;

    if ( hScrollBar && hScrollBar->isVisible() )
    {
        int x = rect.x();
        int y = ( hPos == QwtAxis::XTop )
            ? rect.top() : rect.bottom() - hdim + 1;
        int w = rect.width();

        if ( vScrollBar && vScrollBar->isVisible() )
        {
            if ( vPos == QwtAxis::YLeft )
                x += vdim;
            w -= vdim;
        }

        hScrollBar->setGeometry( x, y, w, hdim );
    }

    if ( vScrollBar && vScrollBar->isVisible() )
    {
        int pos = yAxis();
        if ( vScrollBarPosition() == OppositeToScale )
            pos = oppositeAxis( pos );

        int x = ( vPos == QwtAxis::YLeft )
            ? rect.left() : rect.right() - vdim + 1;
        int y = rect.y();

        int h = rect.height();

        if ( hScrollBar && hScrollBar->isVisible() )
        {
            if ( hPos == QwtAxis::XTop )
                y += hdim;

            h -= hdim;
        }

        vScrollBar->setGeometry( x, y, vdim, h );
    }

    if ( hScrollBar && hScrollBar->isVisible() &&
        vScrollBar && vScrollBar->isVisible() )
    {
        if ( m_cornerWidget )
        {
            QRect cornerRect(
                vScrollBar->pos().x(), hScrollBar->pos().y(),
                vdim, hdim );
            m_cornerWidget->setGeometry( cornerRect );
        }
    }
}

void ScrollZoomer::scrollBarMoved(
    Qt::Orientation o, double min, double max )
{
    Q_UNUSED( max );

    if ( o == Qt::Horizontal )
        moveTo( QPointF( min, zoomRect().top() ) );
    else
        moveTo( QPointF( zoomRect().left(), min ) );

    Q_EMIT zoomed( zoomRect() );
}

int ScrollZoomer::oppositeAxis( int axis ) const
{
    using namespace QwtAxis;

    switch( axis )
    {
        case XBottom:
            return XTop;

        case XTop:
            return XBottom;

        case YLeft:
            return YRight;

        case YRight:
            return YLeft;

        default:
            ;
    }

    return axis;
}

#include "moc_ScrollZoomer.cpp"
