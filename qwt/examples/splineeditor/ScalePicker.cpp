/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "ScalePicker.h"

#include <QwtPlot>
#include <QwtScaleWidget>
#include <QwtScaleMap>

#include <QMouseEvent>
#include <qmath.h>

ScalePicker::ScalePicker( QwtPlot* plot )
    : QObject( plot )
{
    for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
    {
        QwtScaleWidget* scaleWidget = plot->axisWidget( axisPos );
        if ( scaleWidget )
            scaleWidget->installEventFilter( this );
    }
}

bool ScalePicker::eventFilter( QObject* object, QEvent* event )
{
    if ( event->type() == QEvent::MouseButtonPress )
    {
        QwtScaleWidget* scaleWidget = qobject_cast< QwtScaleWidget* >( object );
        if ( scaleWidget )
        {
            QMouseEvent* mouseEvent = static_cast< QMouseEvent* >( event );
            mouseClicked( scaleWidget, mouseEvent->pos() );

            return true;
        }
    }

    return QObject::eventFilter( object, event );
}

void ScalePicker::mouseClicked( const QwtScaleWidget* scale, const QPoint& pos )
{
    QRect rect = scaleRect( scale );

    int margin = 20; // 20 pixels tolerance
    rect.setRect( rect.x() - margin, rect.y() - margin,
        rect.width() + 2 * margin, rect.height() + 2 * margin );

    if ( rect.contains( pos ) ) // No click on the title
    {
        // translate the position in a value on the scale

        double value = 0.0;
        int axis = -1;

        const QwtScaleDraw* sd = scale->scaleDraw();
        switch( scale->alignment() )
        {
            case QwtScaleDraw::LeftScale:
            {
                value = sd->scaleMap().invTransform( pos.y() );
                axis = QwtAxis::YLeft;
                break;
            }
            case QwtScaleDraw::RightScale:
            {
                value = sd->scaleMap().invTransform( pos.y() );
                axis = QwtAxis::YRight;
                break;
            }
            case QwtScaleDraw::BottomScale:
            {
                value = sd->scaleMap().invTransform( pos.x() );
                axis = QwtAxis::XBottom;
                break;
            }
            case QwtScaleDraw::TopScale:
            {
                value = sd->scaleMap().invTransform( pos.x() );
                axis = QwtAxis::XTop;
                break;
            }
        }
        Q_EMIT clicked( axis, value );
    }
}

// The rect of a scale without the title
QRect ScalePicker::scaleRect( const QwtScaleWidget* scale ) const
{
    const int bld = scale->margin();
    const int mjt = qCeil( scale->scaleDraw()->maxTickLength() );
    const int sbd = scale->startBorderDist();
    const int ebd = scale->endBorderDist();

    QRect rect;
    switch( scale->alignment() )
    {
        case QwtScaleDraw::LeftScale:
        {
            rect.setRect( scale->width() - bld - mjt, sbd,
                mjt, scale->height() - sbd - ebd );
            break;
        }
        case QwtScaleDraw::RightScale:
        {
            rect.setRect( bld, sbd,
                mjt, scale->height() - sbd - ebd );
            break;
        }
        case QwtScaleDraw::BottomScale:
        {
            rect.setRect( sbd, bld,
                scale->width() - sbd - ebd, mjt );
            break;
        }
        case QwtScaleDraw::TopScale:
        {
            rect.setRect( sbd, scale->height() - bld - mjt,
                scale->width() - sbd - ebd, mjt );
            break;
        }
    }
    return rect;
}

#include "moc_ScalePicker.cpp"
