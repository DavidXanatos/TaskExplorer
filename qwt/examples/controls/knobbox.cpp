/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "KnobBox.h"

#include <QwtKnob>
#include <QwtScaleEngine>
#include <QwtTransform>

#include <QLabel>
#include <QLayout>

KnobBox::KnobBox( QWidget* parent, int knobType )
    : QWidget( parent )
{
    m_knob = createKnob( knobType );
    m_knob->setKnobWidth( 100 );

    m_label = new QLabel( this );
    m_label->setAlignment( Qt::AlignCenter );

    QVBoxLayout* layout = new QVBoxLayout( this );;
    layout->setSpacing( 0 );
    layout->addWidget( m_knob, 10 );
    layout->addWidget( m_label );
    layout->addStretch( 10 );

    connect( m_knob, SIGNAL(valueChanged(double)),
        this, SLOT(setNum(double)) );

    setNum( m_knob->value() );
}

QwtKnob* KnobBox::createKnob( int knobType ) const
{
    QwtKnob* knob = new QwtKnob();
    knob->setTracking( true );

    switch( knobType )
    {
        case 0:
        {
            knob->setKnobStyle( QwtKnob::Sunken );
            knob->setMarkerStyle( QwtKnob::Nub );
            knob->setWrapping( true );
            knob->setNumTurns( 4 );
            knob->setScaleStepSize( 10.0 );
            knob->setScale( 0, 400 );
            knob->setTotalSteps( 400 );
            break;
        }
        case 1:
        {
            knob->setKnobStyle( QwtKnob::Sunken );
            knob->setMarkerStyle( QwtKnob::Dot );
            break;
        }
        case 2:
        {
            knob->setKnobStyle( QwtKnob::Sunken );
            knob->setMarkerStyle( QwtKnob::Tick );

            QwtLinearScaleEngine* scaleEngine = new QwtLinearScaleEngine( 2 );
            scaleEngine->setTransformation( new QwtPowerTransform( 2 ) );
            knob->setScaleEngine( scaleEngine );

            QList< double > ticks[ QwtScaleDiv::NTickTypes ];
            ticks[ QwtScaleDiv::MajorTick ] << 0 << 4
                                            << 16 << 32 << 64 << 96 << 128;
            ticks[ QwtScaleDiv::MediumTick ] << 24 << 48 << 80 << 112;
            ticks[ QwtScaleDiv::MinorTick ]
                << 0.5 << 1 << 2
                << 7 << 10 << 13
                << 20 << 28
                << 40 << 56
                << 72 << 88
                << 104 << 120;

            knob->setScale( QwtScaleDiv( 0, 128, ticks ) );

            knob->setTotalSteps( 100 );
            knob->setStepAlignment( false );
            knob->setSingleSteps( 1 );
            knob->setPageSteps( 5 );

            break;
        }
        case 3:
        {
            knob->setKnobStyle( QwtKnob::Flat );
            knob->setMarkerStyle( QwtKnob::Notch );
            knob->setScaleEngine( new QwtLogScaleEngine() );
            knob->setScaleStepSize( 1.0 );
            knob->setScale( 0.1, 1000.0 );
            knob->setScaleMaxMinor( 10 );
            break;
        }
        case 4:
        {
            knob->setKnobStyle( QwtKnob::Raised );
            knob->setMarkerStyle( QwtKnob::Dot );
            knob->setWrapping( true );
            break;
        }
        case 5:
        {
            knob->setKnobStyle( QwtKnob::Styled );
            knob->setMarkerStyle( QwtKnob::Triangle );
            knob->setTotalAngle( 180.0 );
            knob->setScale( 100, -100 );
            break;
        }
    }

    return knob;
}

void KnobBox::setNum( double v )
{
    QString text;
    text.setNum( v, 'f', 2 );

    m_label->setText( text );
}

#include "moc_KnobBox.cpp"
