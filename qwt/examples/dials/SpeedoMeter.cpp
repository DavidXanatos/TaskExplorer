/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "SpeedoMeter.h"

#include <QwtDialNeedle>
#include <QwtRoundScaleDraw>

#include <QPainter>

SpeedoMeter::SpeedoMeter( QWidget* parent )
    : QwtDial( parent )
    , m_label( "km/h" )
{
    QwtRoundScaleDraw* scaleDraw = new QwtRoundScaleDraw();
    scaleDraw->setSpacing( 8 );
    scaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
    scaleDraw->setTickLength( QwtScaleDiv::MinorTick, 0 );
    scaleDraw->setTickLength( QwtScaleDiv::MediumTick, 4 );
    scaleDraw->setTickLength( QwtScaleDiv::MajorTick, 8 );
    setScaleDraw( scaleDraw );

    setWrapping( false );
    setReadOnly( true );

    setOrigin( 135.0 );
    setScaleArc( 0.0, 270.0 );

    QwtDialSimpleNeedle* needle = new QwtDialSimpleNeedle(
        QwtDialSimpleNeedle::Arrow, true, Qt::red,
        QColor( Qt::gray ).lighter( 130 ) );
    setNeedle( needle );
}

void SpeedoMeter::setLabel( const QString& label )
{
    m_label = label;
    update();
}

QString SpeedoMeter::label() const
{
    return m_label;
}

void SpeedoMeter::drawScaleContents( QPainter* painter,
    const QPointF& center, double radius ) const
{
    QRectF rect( 0.0, 0.0, 2.0 * radius, 2.0 * radius - 10.0 );
    rect.moveCenter( center );

    const QColor color = palette().color( QPalette::Text );
    painter->setPen( color );

    const int flags = Qt::AlignBottom | Qt::AlignHCenter;
    painter->drawText( rect, flags, m_label );
}
