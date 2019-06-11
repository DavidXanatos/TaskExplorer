#include "curvetracker.h"
#include <qwt_picker_machine.h>
#include <qwt_series_data.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>

struct compareX
{
    inline bool operator()( const double x, const QPointF &pos ) const
    {
        return ( x < pos.x() );
    }
};

CurveTracker::CurveTracker( QWidget *canvas ):
    QwtPlotPicker( canvas )
{
    setTrackerMode( QwtPlotPicker::ActiveOnly );
    setRubberBand( VLineRubberBand );

    setStateMachine( new QwtPickerDragPointMachine() );
}

QRect CurveTracker::trackerRect( const QFont &font ) const
{
    QRect r = QwtPlotPicker::trackerRect( font );
    
    // align r to the first curve

    const QwtPlotItemList curves = plot()->itemList( QwtPlotItem::Rtti_PlotCurve );
    if ( curves.size() > 0 )
    {
        QPointF pos = invTransform( trackerPosition() );

        const QLineF line = curveLineAt(    
            static_cast<const QwtPlotCurve *>( curves[0] ), pos.x() );
        if ( !line.isNull() )
        {
            const double curveY = line.pointAt(
                ( pos.x() - line.p1().x() ) / line.dx() ).y();

            pos.setY( curveY );
            pos = transform( pos );

            r.moveBottom( pos.y() );
        }
    }

    return r;
}

QwtText CurveTracker::trackerTextF( const QPointF &pos ) const
{
    QwtText trackerText;

    trackerText.setColor( Qt::black );

    QColor c( "#333333" );
    trackerText.setBorderPen( QPen( c, 2 ) );
    c.setAlpha( 200 );
    trackerText.setBackgroundBrush( c );

    QString info;

    const QwtPlotItemList curves = 
        plot()->itemList( QwtPlotItem::Rtti_PlotCurve );

    for ( int i = 0; i < curves.size(); i++ )
    {
        const QString curveInfo = curveInfoAt( 
            static_cast<const QwtPlotCurve *>( curves[i] ), pos );

        if ( !curveInfo.isEmpty() )
        {
            if ( !info.isEmpty() )
                info += "<br>";

            info += curveInfo;
        }
    }

    trackerText.setText( info );
    return trackerText;
}

QString CurveTracker::curveInfoAt( 
    const QwtPlotCurve *curve, const QPointF &pos ) const
{
    const QLineF line = curveLineAt( curve, pos.x() );
    if ( line.isNull() )
        return QString::null;

    const double y = line.pointAt( 
        ( pos.x() - line.p1().x() ) / line.dx() ).y();

    QString info( "<font color=""%1"">%2</font>" );
    return info.arg( curve->pen().color().name() ).arg( y );
}

QLineF CurveTracker::curveLineAt( 
    const QwtPlotCurve *curve, double x ) const
{
    QLineF line;

    if ( curve->dataSize() >= 2 )
    {
        const QRectF br = curve->boundingRect();
        if ( ( br.width() > 0 ) && ( x >= br.left() ) && ( x <= br.right() ) )
        {
            int index = qwtUpperSampleIndex<QPointF>( 
                *curve->data(), x, compareX() );

            if ( index == -1 && 
                x == curve->sample( curve->dataSize() - 1 ).x() )
            {
                // the last sample is excluded from qwtUpperSampleIndex
                index = curve->dataSize() - 1;
            }

            if ( index > 0 )
            {
                line.setP1( curve->sample( index - 1 ) );
                line.setP2( curve->sample( index ) );
            }
        }
    }
    
    return line;
}
