/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "SliderBox.h"

#include <QwtSlider>
#include <QwtScaleEngine>
#include <QwtTransform>
#include <QwtPainter>

#include <QLabel>
#include <QLayout>

SliderBox::SliderBox( int sliderType, QWidget* parent )
    : QWidget( parent )
{
    m_slider = createSlider( sliderType );

    QFlags< Qt::AlignmentFlag > alignment;

    if ( m_slider->orientation() == Qt::Horizontal )
    {
        if ( m_slider->scalePosition() == QwtSlider::TrailingScale )
            alignment = Qt::AlignBottom;
        else
            alignment = Qt::AlignTop;

        alignment |= Qt::AlignHCenter;
    }
    else
    {
        if ( m_slider->scalePosition() == QwtSlider::TrailingScale )
            alignment = Qt::AlignRight;
        else
            alignment = Qt::AlignLeft;

        alignment |= Qt::AlignVCenter;
    }

    m_label = new QLabel( this );
    m_label->setAlignment( alignment );

    const int labelWidth = QwtPainter::horizontalAdvance(
        m_label->fontMetrics(), "10000.9" );
    m_label->setFixedWidth( labelWidth );

    connect( m_slider, SIGNAL(valueChanged(double)), SLOT(setNum(double)) );

    QBoxLayout* layout;
    if ( m_slider->orientation() == Qt::Horizontal )
        layout = new QHBoxLayout( this );
    else
        layout = new QVBoxLayout( this );

    layout->addWidget( m_slider );
    layout->addWidget( m_label );

    setNum( m_slider->value() );
}

QwtSlider* SliderBox::createSlider( int sliderType ) const
{
    QwtSlider* slider = new QwtSlider();

    switch( sliderType )
    {
        case 0:
        {
            slider->setOrientation( Qt::Horizontal );
            slider->setScalePosition( QwtSlider::TrailingScale );
            slider->setTrough( true );
            slider->setGroove( false );
            slider->setSpacing( 0 );
            slider->setHandleSize( QSize( 30, 16 ) );
            slider->setScale( 10.0, -10.0 );
            slider->setTotalSteps( 8 );
            slider->setSingleSteps( 1 );
            slider->setPageSteps( 1 );
            slider->setWrapping( true );
            break;
        }
        case 1:
        {
            slider->setOrientation( Qt::Horizontal );
            slider->setScalePosition( QwtSlider::NoScale );
            slider->setTrough( true );
            slider->setGroove( true );
            slider->setScale( 0.0, 1.0 );
            slider->setTotalSteps( 100 );
            slider->setSingleSteps( 1 );
            slider->setPageSteps( 5 );
            break;
        }
        case 2:
        {
            slider->setOrientation( Qt::Horizontal );
            slider->setScalePosition( QwtSlider::LeadingScale );
            slider->setTrough( false );
            slider->setGroove( true );
            slider->setHandleSize( QSize( 12, 25 ) );
            slider->setScale( 1000.0, 3000.0 );
            slider->setTotalSteps( 200.0 );
            slider->setSingleSteps( 2 );
            slider->setPageSteps( 10 );
            break;
        }
        case 3:
        {
            slider->setOrientation( Qt::Horizontal );
            slider->setScalePosition( QwtSlider::TrailingScale );
            slider->setTrough( true );
            slider->setGroove( true );

            QwtLinearScaleEngine* scaleEngine = new QwtLinearScaleEngine( 2 );
            scaleEngine->setTransformation( new QwtPowerTransform( 2 ) );
            slider->setScaleEngine( scaleEngine );
            slider->setScale( 0.0, 128.0 );
            slider->setTotalSteps( 100 );
            slider->setStepAlignment( false );
            slider->setSingleSteps( 1 );
            slider->setPageSteps( 5 );
            break;
        }
        case 4:
        {
            slider->setOrientation( Qt::Vertical );
            slider->setScalePosition( QwtSlider::TrailingScale );
            slider->setTrough( false );
            slider->setGroove( true );
            slider->setScale( 100.0, 0.0 );
            slider->setInvertedControls( true );
            slider->setTotalSteps( 100 );
            slider->setPageSteps( 5 );
            slider->setScaleMaxMinor( 5 );
            break;
        }
        case 5:
        {
            slider->setOrientation( Qt::Vertical );
            slider->setScalePosition( QwtSlider::NoScale );
            slider->setTrough( true );
            slider->setGroove( false );
            slider->setScale( 0.0, 100.0 );
            slider->setTotalSteps( 100 );
            slider->setPageSteps( 10 );
            break;
        }
        case 6:
        {
            slider->setOrientation( Qt::Vertical );
            slider->setScalePosition( QwtSlider::LeadingScale );
            slider->setTrough( true );
            slider->setGroove( true );
            slider->setScaleEngine( new QwtLogScaleEngine );
            slider->setStepAlignment( false );
            slider->setHandleSize( QSize( 20, 32 ) );
            slider->setBorderWidth( 1 );
            slider->setScale( 1.0, 1.0e4 );
            slider->setTotalSteps( 100 );
            slider->setPageSteps( 10 );
            slider->setScaleMaxMinor( 9 );
            break;
        }
    }

    if ( slider )
    {
        QString name( "Slider %1" );
        slider->setObjectName( name.arg( sliderType ) );
    }

    return slider;
}

void SliderBox::setNum( double v )
{
    QString text;
    text.setNum( v, 'f', 2 );

    m_label->setText( text );
}

#include "moc_SliderBox.cpp"
