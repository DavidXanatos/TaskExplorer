/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "TransformPlot.h"

#include <QwtPlotCurve>
#include <QwtSyntheticPointData>
#include <QwtTransform>
#include <QwtLegend>
#include <QwtLegendLabel>

namespace
{
    class TransformData : public QwtSyntheticPointData
    {
      public:
        TransformData( QwtTransform* transform )
            : QwtSyntheticPointData( 200 )
            , m_transform( transform )
        {
        }

        virtual ~TransformData()
        {
            delete m_transform;
        }

        const QwtTransform* transform() const
        {
            return m_transform;
        }

        virtual double y( double x ) const QWT_OVERRIDE
        {
            const double min = 10.0;
            const double max = 1000.0;

            const double value = min + x * ( max - min );

            const double s1 = m_transform->transform( min );
            const double s2 = m_transform->transform( max );
            const double s = m_transform->transform( value );

            return ( s - s1 ) / ( s2 - s1 );
        }

      private:
        const QwtTransform* m_transform;
    };
}

TransformPlot::TransformPlot( QWidget* parent )
    : QwtPlot( parent )
{
    setTitle( "Transformations" );
    setCanvasBackground( Qt::white );

    setAxisScale( QwtAxis::XBottom, 0.0, 1.0 );
    setAxisScale( QwtAxis::YLeft, 0.0, 1.0 );

    QwtLegend* legend = new QwtLegend();
    legend->setDefaultItemMode( QwtLegendData::Checkable );
    insertLegend( legend, QwtPlot::RightLegend );

    connect( legend, SIGNAL(checked(const QVariant&,bool,int)),
        this, SLOT(legendChecked(const QVariant&,bool)) );
}

void TransformPlot::insertTransformation(
    const QString& title, const QColor& color, QwtTransform* transform )
{
    QwtPlotCurve* curve = new QwtPlotCurve( title );
    curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
    curve->setPen( color, 2 );
    curve->setData( new TransformData( transform ) );
    curve->attach( this );
}

void TransformPlot::legendChecked( const QVariant& itemInfo, bool on )
{
    QwtPlotItem* plotItem = infoToItem( itemInfo );

    setLegendChecked( plotItem );

    if ( on && plotItem->rtti() == QwtPlotItem::Rtti_PlotCurve )
    {
        QwtPlotCurve* curve = static_cast< QwtPlotCurve* >( plotItem );
        TransformData* curveData = static_cast< TransformData* >( curve->data() );

        Q_EMIT selected( curveData->transform()->copy() );
    }
}

void TransformPlot::setLegendChecked( QwtPlotItem* plotItem )
{
    const QwtPlotItemList items = itemList();
    for ( int i = 0; i < items.size(); i++ )
    {
        QwtPlotItem* item = items[ i ];
        if ( item->testItemAttribute( QwtPlotItem::Legend ) )
        {
            QwtLegend* lgd = qobject_cast< QwtLegend* >( legend() );

            QwtLegendLabel* label = qobject_cast< QwtLegendLabel* >(
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

#include "moc_TransformPlot.cpp"
