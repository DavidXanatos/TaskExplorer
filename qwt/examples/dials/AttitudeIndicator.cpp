/*****************************************************************************
* Qwt Examples - Copyright (C) 2002 Uwe Rathmann
* This file may be used under the terms of the 3-clause BSD License
*****************************************************************************/

#include "AttitudeIndicator.h"

#include <QwtPointPolar>
#include <QwtRoundScaleDraw>
#include <QwtDialNeedle>

#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

namespace
{
    class Needle : public QwtDialNeedle
    {
      public:

        Needle( const QColor& color )
        {
            QPalette palette;
            palette.setColor( QPalette::Text, color );
            setPalette( palette );
        }

      protected:

        virtual void drawNeedle( QPainter* painter,
            double length, QPalette::ColorGroup colorGroup ) const QWT_OVERRIDE
        {
            double triangleSize = length * 0.1;
            double pos = length - 2.0;

            QPainterPath path;
            path.moveTo( pos, 0 );
            path.lineTo( pos - 2 * triangleSize, triangleSize );
            path.lineTo( pos - 2 * triangleSize, -triangleSize );
            path.closeSubpath();

            painter->setBrush( palette().brush( colorGroup, QPalette::Text ) );
            painter->drawPath( path );

            double l = length - 2;
            painter->setPen( QPen( palette().color( colorGroup, QPalette::Text ), 3 ) );
            painter->drawLine( QPointF( 0.0, -l ), QPointF( 0.0, l ) );
        }
    };
}

AttitudeIndicator::AttitudeIndicator( QWidget* parent )
    : QwtDial( parent )
    , m_gradient( 0.0 )
{
    QwtRoundScaleDraw* scaleDraw = new QwtRoundScaleDraw();
    scaleDraw->enableComponent( QwtAbstractScaleDraw::Backbone, false );
    scaleDraw->enableComponent( QwtAbstractScaleDraw::Labels, false );
    setScaleDraw( scaleDraw );

    setMode( RotateScale );
    setWrapping( true );

    setOrigin( 270.0 );

    setScaleMaxMinor( 0 );
    setScaleStepSize( 30.0 );
    setScale( 0.0, 360.0 );

    const QColor color = palette().color( QPalette::Text );
    setNeedle( new Needle( color ) );
}

void AttitudeIndicator::setGradient( double gradient )
{
    if ( gradient < -1.0 )
        gradient = -1.0;
    else if ( gradient > 1.0 )
        gradient = 1.0;

    if ( m_gradient != gradient )
    {
        m_gradient = gradient;
        update();
    }
}

void AttitudeIndicator::drawScale( QPainter* painter,
    const QPointF& center, double radius ) const
{
    const double offset = 4.0;

    const QPointF p0 = qwtPolar2Pos( center, offset, 1.5 * M_PI );

    const double w = innerRect().width();

    QPainterPath path;
    path.moveTo( qwtPolar2Pos( p0, w, 0.0 ) );
    path.lineTo( qwtPolar2Pos( path.currentPosition(), 2 * w, M_PI ) );
    path.lineTo( qwtPolar2Pos( path.currentPosition(), w, 0.5 * M_PI ) );
    path.lineTo( qwtPolar2Pos( path.currentPosition(), w, 0.0 ) );

    painter->save();
    painter->setClipPath( path ); // swallow 180 - 360 degrees

    QwtDial::drawScale( painter, center, radius );

    painter->restore();
}

void AttitudeIndicator::drawScaleContents(
    QPainter* painter, const QPointF&, double ) const
{
    int dir = 360 - qRound( origin() - value() ); // counter clockwise
    int arc = 90 + qRound( gradient() * 90 );

    const QColor skyColor( 38, 151, 221 );

    painter->save();
    painter->setBrush( skyColor );
    painter->drawChord( scaleInnerRect(),
        ( dir - arc ) * 16, 2 * arc * 16 );
    painter->restore();
}

void AttitudeIndicator::keyPressEvent( QKeyEvent* event )
{
    switch( event->key() )
    {
        case Qt::Key_Plus:
        {
            setGradient( gradient() + 0.05 );
            break;
        }
        case Qt::Key_Minus:
        {
            setGradient( gradient() - 0.05 );
            break;
        }
        default:
            QwtDial::keyPressEvent( event );
    }
}

#include "moc_AttitudeIndicator.cpp"
