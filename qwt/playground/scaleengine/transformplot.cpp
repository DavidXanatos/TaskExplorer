#include "transformplot.h"
#include <qwt_curve_fitter.h>
#include <qwt_plot_curve.h>
#include <qwt_point_data.h>
#include <qwt_transform.h>
#include <qwt_legend.h>
#include <qwt_legend_label.h>

class TransformData: public QwtSyntheticPointData
{
public:
    TransformData( QwtTransform *transform ):
        QwtSyntheticPointData( 200 ),
        d_transform( transform )
    {
    }

    virtual ~TransformData()
    {
        delete d_transform;
    }

    const QwtTransform *transform() const
    {
        return d_transform;
    }

    virtual double y( double x ) const
    {
        const double min = 10.0;
        const double max = 1000.0;

        const double value = min + x * ( max - min );

        const double s1 = d_transform->transform( min );
        const double s2 = d_transform->transform( max );
        const double s = d_transform->transform( value );

        return ( s - s1 ) / ( s2 - s1 );
    }

private:
    QwtTransform *d_transform;
};

TransformPlot::TransformPlot( QWidget *parent ):
    QwtPlot( parent )
{
    setTitle( "Transformations" );
    setCanvasBackground( Qt::white );

    setAxisScale( QwtPlot::xBottom, 0.0, 1.0 );
    setAxisScale( QwtPlot::yLeft, 0.0, 1.0 );

    QwtLegend *legend = new QwtLegend();
    legend->setDefaultItemMode( QwtLegendData::Checkable );
    insertLegend( legend, QwtPlot::RightLegend );

    connect( legend, SIGNAL( checked( const QVariant &, bool, int ) ),
        this, SLOT( legendChecked( const QVariant &, bool ) ) );
}

void TransformPlot::insertTransformation( 
    const QString &title, const QColor &color, QwtTransform *transform )
{
    QwtPlotCurve *curve = new QwtPlotCurve( title );
    curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
    curve->setPen( color, 2 );
    curve->setData( new TransformData( transform ) );
    curve->attach( this );
}

void TransformPlot::legendChecked( const QVariant &itemInfo, bool on )
{
    QwtPlotItem *plotItem = infoToItem( itemInfo );

    setLegendChecked( plotItem );

    if ( on && plotItem->rtti() == QwtPlotItem::Rtti_PlotCurve )
    {
        QwtPlotCurve *curve = static_cast<QwtPlotCurve *>( plotItem );
        TransformData *data = static_cast<TransformData *>( curve->data() );

        Q_EMIT selected( data->transform()->copy() );
    }
}

void TransformPlot::setLegendChecked( QwtPlotItem *plotItem )
{
    const QwtPlotItemList items = itemList();
    for ( int i = 0; i < items.size(); i++ )
    {
        QwtPlotItem *item = items[ i ];
        if ( item->testItemAttribute( QwtPlotItem::Legend ) )
        {
            QwtLegend *lgd = qobject_cast<QwtLegend *>( legend() );

            QwtLegendLabel *label = qobject_cast< QwtLegendLabel *>( 
                lgd->legendWidget( itemToInfo( item ) ) );
            if ( label )
            {
                lgd->blockSignals( true );
                label->setChecked( item == plotItem );
                lgd->blockSignals( false );
            }
        }
    }
}
