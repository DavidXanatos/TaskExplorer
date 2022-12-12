/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "WheelBox.h"

#include <QwtWheel>
#include <QwtThermo>
#include <QwtScaleEngine>
#include <QwtTransform>
#include <QwtColorMap>

#include <QLabel>
#include <QLayout>

WheelBox::WheelBox( Qt::Orientation orientation, int type, QWidget* parent )
    : QWidget( parent )
{
    QWidget* box = createBox( orientation, type );
    m_label = new QLabel( this );
    m_label->setAlignment( Qt::AlignHCenter | Qt::AlignTop );

    QBoxLayout* layout = new QVBoxLayout( this );
    layout->addWidget( box );
    layout->addWidget( m_label );

    setNum( m_wheel->value() );

    connect( m_wheel, SIGNAL(valueChanged(double)),
        this, SLOT(setNum(double)) );
}

QWidget* WheelBox::createBox( Qt::Orientation orientation, int type )
{
    m_wheel = new QwtWheel();
    m_wheel->setValue( 80 );
    m_wheel->setWheelWidth( 20 );
    m_wheel->setMass( 1.0 );

    m_thermo = new QwtThermo();
    m_thermo->setOrientation( orientation );

    if ( orientation == Qt::Horizontal )
    {
        m_thermo->setScalePosition( QwtThermo::LeadingScale );
        m_wheel->setOrientation( Qt::Vertical );
    }
    else
    {
        m_thermo->setScalePosition( QwtThermo::TrailingScale );
        m_wheel->setOrientation( Qt::Horizontal );
    }

    switch( type )
    {
        case 0:
        {
            QwtLinearColorMap* colorMap = new QwtLinearColorMap();
            colorMap->setColorInterval( Qt::blue, Qt::red );
            m_thermo->setColorMap( colorMap );

            break;
        }
        case 1:
        {
            QwtLinearColorMap* colorMap = new QwtLinearColorMap();
            colorMap->setMode( QwtLinearColorMap::FixedColors );

            int idx = 4;

            colorMap->setColorInterval( Qt::GlobalColor( idx ),
                Qt::GlobalColor( idx + 10 ) );
            for ( int i = 1; i < 10; i++ )
            {
                colorMap->addColorStop( i / 10.0,
                    Qt::GlobalColor( idx + i ) );
            }

            m_thermo->setColorMap( colorMap );
            break;
        }
        case 2:
        {
            m_wheel->setRange( 10, 1000 );
            m_wheel->setSingleStep( 1.0 );

            m_thermo->setScaleEngine( new QwtLogScaleEngine );
            m_thermo->setScaleMaxMinor( 10 );

            m_thermo->setFillBrush( Qt::darkCyan );
            m_thermo->setAlarmBrush( Qt::magenta );
            m_thermo->setAlarmLevel( 500.0 );

            m_wheel->setValue( 800 );

            break;
        }
        case 3:
        {
            m_wheel->setRange( -1000, 1000 );
            m_wheel->setSingleStep( 1.0 );
            m_wheel->setPalette( QColor( "Tan" ) );

            QwtLinearScaleEngine* scaleEngine = new QwtLinearScaleEngine();
            scaleEngine->setTransformation( new QwtPowerTransform( 2 ) );

            m_thermo->setScaleMaxMinor( 5 );
            m_thermo->setScaleEngine( scaleEngine );

            QPalette pal = palette();
            pal.setColor( QPalette::Base, Qt::darkGray );
            pal.setColor( QPalette::ButtonText, QColor( "darkKhaki" ) );

            m_thermo->setPalette( pal );
            break;
        }
        case 4:
        {
            m_wheel->setRange( -100, 300 );
            m_wheel->setInverted( true );

            QwtLinearColorMap* colorMap = new QwtLinearColorMap();
            colorMap->setColorInterval( Qt::darkCyan, Qt::yellow );
            m_thermo->setColorMap( colorMap );

            m_wheel->setValue( 243 );

            break;
        }
        case 5:
        {
            m_thermo->setFillBrush( Qt::darkCyan );
            m_thermo->setAlarmBrush( Qt::magenta );
            m_thermo->setAlarmLevel( 60.0 );

            break;
        }
        case 6:
        {
            m_thermo->setOriginMode( QwtThermo::OriginMinimum );
            m_thermo->setFillBrush( QBrush( "DarkSlateBlue" ) );
            m_thermo->setAlarmBrush( QBrush( "DarkOrange" ) );
            m_thermo->setAlarmLevel( 60.0 );

            break;
        }
        case 7:
        {
            m_wheel->setRange( -100, 100 );

            m_thermo->setOriginMode( QwtThermo::OriginCustom );
            m_thermo->setOrigin( 0.0 );
            m_thermo->setFillBrush( Qt::darkBlue );

            break;
        }
    }

    double min = m_wheel->minimum();
    double max = m_wheel->maximum();

    if ( m_wheel->isInverted() )
        qSwap( min, max );

    m_thermo->setScale( min, max );
    m_thermo->setValue( m_wheel->value() );

    connect( m_wheel, SIGNAL(valueChanged(double)),
        m_thermo, SLOT(setValue(double)) );

    QWidget* box = new QWidget();

    QBoxLayout* layout;

    if ( orientation == Qt::Horizontal )
        layout = new QHBoxLayout( box );
    else
        layout = new QVBoxLayout( box );

    layout->addWidget( m_thermo, Qt::AlignCenter );
    layout->addWidget( m_wheel );

    return box;
}

void WheelBox::setNum( double v )
{
    QString text;
    text.setNum( v, 'f', 2 );

    m_label->setText( text );
}

#include "moc_WheelBox.cpp"
