/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Knob.h"

#include <QwtMath>
#include <QwtKnob>
#include <QwtRoundScaleDraw>
#include <QwtScaleEngine>

#include <QLabel>
#include <QResizeEvent>
#include <qmath.h>

Knob::Knob( const QString& title, double min, double max, QWidget* parent )
    : QWidget( parent )
{
    QFont font( "Helvetica", 10 );

    m_knob = new QwtKnob( this );
    m_knob->setFont( font );

    QwtScaleDiv scaleDiv =
        m_knob->scaleEngine()->divideScale( min, max, 5, 3 );

    QList< double > ticks = scaleDiv.ticks( QwtScaleDiv::MajorTick );
    if ( ticks.size() > 0 && ticks[0] > min )
    {
        if ( ticks.first() > min )
            ticks.prepend( min );
        if ( ticks.last() < max )
            ticks.append( max );
    }
    scaleDiv.setTicks( QwtScaleDiv::MajorTick, ticks );
    m_knob->setScale( scaleDiv );

    m_knob->setKnobWidth( 50 );

    font.setBold( true );
    m_label = new QLabel( title, this );
    m_label->setFont( font );
    m_label->setAlignment( Qt::AlignTop | Qt::AlignHCenter );

    setSizePolicy( QSizePolicy::MinimumExpanding,
        QSizePolicy::MinimumExpanding );

    connect( m_knob, SIGNAL(valueChanged(double)),
        this, SIGNAL(valueChanged(double)) );
}

QSize Knob::sizeHint() const
{
    QSize sz1 = m_knob->sizeHint();
    QSize sz2 = m_label->sizeHint();

    const int w = qMax( sz1.width(), sz2.width() );
    const int h = sz1.height() + sz2.height();

    int off = qCeil( m_knob->scaleDraw()->extent( m_knob->font() ) );
    off -= 15; // spacing

    return QSize( w, h - off );
}

void Knob::setValue( double value )
{
    m_knob->setValue( value );
}

double Knob::value() const
{
    return m_knob->value();
}

void Knob::setTheme( const QColor& color )
{
    m_knob->setPalette( color );
}

QColor Knob::theme() const
{
    return m_knob->palette().color( QPalette::Window );
}

void Knob::resizeEvent( QResizeEvent* event )
{
    const QSize sz = event->size();
    const QSize hint = m_label->sizeHint();

    m_label->setGeometry( 0, sz.height() - hint.height(),
        sz.width(), hint.height() );

    const int knobHeight = m_knob->sizeHint().height();

    int off = qCeil( m_knob->scaleDraw()->extent( m_knob->font() ) );
    off -= 15; // spacing

    m_knob->setGeometry( 0, m_label->pos().y() - knobHeight + off,
        sz.width(), knobHeight );
}

#include "moc_Knob.cpp"
