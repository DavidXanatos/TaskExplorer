/******************************************************************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "TunerBox.h"

#include <QwtWheel>
#include <QwtSlider>
#include <QwtThermo>
#include <QwtMath>

#include <QLayout>
#include <QLabel>

class TuningThermo : public QWidget
{
  public:
    TuningThermo( QWidget* parent )
        : QWidget( parent )
    {
        m_thermo = new QwtThermo( this );
        m_thermo->setOrientation( Qt::Horizontal );
        m_thermo->setScalePosition( QwtThermo::NoScale );
        m_thermo->setScale( 0.0, 1.0 );
        m_thermo->setFillBrush( Qt::green );

        QLabel* label = new QLabel( "Tuning", this );
        label->setAlignment( Qt::AlignCenter );

        QVBoxLayout* layout = new QVBoxLayout( this );
        layout->setContentsMargins( QMargins() );
        layout->addWidget( m_thermo );
        layout->addWidget( label );

        setFixedWidth( 3 * label->sizeHint().width() );
    }

    void setValue( double value )
    {
        m_thermo->setValue( value );
    }

  private:
    QwtThermo* m_thermo;
};

TunerBox::TunerBox( QWidget* parent ):
    QFrame( parent )
{
    const double freqMin = 87.5;
    const double freqMax = 108;

    m_sliderFrequency = new QwtSlider( this );
    m_sliderFrequency->setOrientation( Qt::Horizontal );
    m_sliderFrequency->setScalePosition( QwtSlider::TrailingScale );
    m_sliderFrequency->setScale( freqMin, freqMax );
    m_sliderFrequency->setTotalSteps(
        qRound( ( freqMax - freqMin ) / 0.01 ) );
    m_sliderFrequency->setSingleSteps( 1 );
    m_sliderFrequency->setPageSteps( 10 );
    m_sliderFrequency->setScaleMaxMinor( 5 );
    m_sliderFrequency->setScaleMaxMajor( 12 );
    m_sliderFrequency->setHandleSize( QSize( 80, 20 ) );
    m_sliderFrequency->setBorderWidth( 1 );

    m_thermoTune = new TuningThermo( this );

    m_wheelFrequency = new QwtWheel( this );
    m_wheelFrequency->setMass( 0.5 );
    m_wheelFrequency->setRange( 87.5, 108 );
    m_wheelFrequency->setSingleStep( 0.01 );
    m_wheelFrequency->setPageStepCount( 10 );
    m_wheelFrequency->setTotalAngle( 3600.0 );
    m_wheelFrequency->setFixedHeight( 30 );


    connect( m_wheelFrequency, SIGNAL(valueChanged(double)), SLOT(adjustFreq(double)) );
    connect( m_sliderFrequency, SIGNAL(valueChanged(double)), SLOT(adjustFreq(double)) );

    QVBoxLayout* mainLayout = new QVBoxLayout( this );
    mainLayout->setContentsMargins( 10, 10, 10, 10 );
    mainLayout->setSpacing( 5 );
    mainLayout->addWidget( m_sliderFrequency );

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->setContentsMargins( QMargins() );
    hLayout->addWidget( m_thermoTune, 0 );
    hLayout->addStretch( 5 );
    hLayout->addWidget( m_wheelFrequency, 2 );

    mainLayout->addLayout( hLayout );
}

void TunerBox::adjustFreq( double frq )
{
    const double factor = 13.0 / ( 108 - 87.5 );

    const double x = ( frq - 87.5 ) * factor;
    const double field = qwtSqr( std::sin( x ) * std::cos( 4.0 * x ) );

    m_thermoTune->setValue( field );

    if ( m_sliderFrequency->value() != frq )
        m_sliderFrequency->setValue( frq );
    if ( m_wheelFrequency->value() != frq )
        m_wheelFrequency->setValue( frq );

    Q_EMIT fieldChanged( field );
}

void TunerBox::setFreq( double frq )
{
    m_wheelFrequency->setValue( frq );
}

#include "moc_TunerBox.cpp"
