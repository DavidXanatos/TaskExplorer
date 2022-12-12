/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Panel.h"
#include "Settings.h"

#include <QDateTimeEdit>
#include <QSpinBox>
#include <QLayout>
#include <QLabel>

Panel::Panel( QWidget* parent )
    : QWidget( parent )
{
    // create widgets

    m_startDateTime = new QDateTimeEdit();
    m_startDateTime->setDisplayFormat( "M/d/yyyy h:mm AP :zzz" );
    m_startDateTime->setCalendarPopup( true );

    m_endDateTime = new QDateTimeEdit();
    m_endDateTime->setDisplayFormat( "M/d/yyyy h:mm AP :zzz" );
    m_endDateTime->setCalendarPopup( true );

    m_maxMajorSteps = new QSpinBox();
    m_maxMajorSteps->setRange( 0, 50 );

    m_maxMinorSteps = new QSpinBox();
    m_maxMinorSteps->setRange( 0, 50 );

    m_maxWeeks = new QSpinBox();
    m_maxWeeks->setRange( -1, 100 );
    m_maxWeeks->setSpecialValueText( "Disabled" );

    // layout

    QGridLayout* layout = new QGridLayout( this );
    layout->setAlignment( Qt::AlignLeft | Qt::AlignTop );

    int row = 0;
    layout->addWidget( new QLabel( "From" ), row, 0 );
    layout->addWidget( m_startDateTime, row, 1 );

    row++;
    layout->addWidget( new QLabel( "To" ), row, 0 );
    layout->addWidget( m_endDateTime, row, 1 );

    row++;
    layout->addWidget( new QLabel( "Max. Major Steps" ), row, 0 );
    layout->addWidget( m_maxMajorSteps, row, 1 );

    row++;
    layout->addWidget( new QLabel( "Max. Minor Steps" ), row, 0 );
    layout->addWidget( m_maxMinorSteps, row, 1 );

    row++;
    layout->addWidget( new QLabel( "Max Weeks" ), row, 0 );
    layout->addWidget( m_maxWeeks, row, 1 );

    connect( m_startDateTime,
        SIGNAL(dateTimeChanged(const QDateTime&)), SIGNAL(edited()) );
    connect( m_endDateTime,
        SIGNAL(dateTimeChanged(const QDateTime&)), SIGNAL(edited()) );
    connect( m_maxMajorSteps,
        SIGNAL(valueChanged(int)), SIGNAL(edited()) );
    connect( m_maxMinorSteps,
        SIGNAL(valueChanged(int)), SIGNAL(edited()) );
    connect( m_maxWeeks,
        SIGNAL(valueChanged(int)), SIGNAL(edited()) );
}

void Panel::setSettings( const Settings& settings )
{
    blockSignals( true );

    m_startDateTime->setDateTime( settings.startDateTime );
    m_endDateTime->setDateTime( settings.endDateTime );

    m_maxMajorSteps->setValue( settings.maxMajorSteps );
    m_maxMinorSteps->setValue( settings.maxMinorSteps );
    m_maxWeeks->setValue( settings.maxWeeks );

    blockSignals( false );
}

Settings Panel::settings() const
{
    Settings settings;

    settings.startDateTime = m_startDateTime->dateTime();
    settings.endDateTime = m_endDateTime->dateTime();

    settings.maxMajorSteps = m_maxMajorSteps->value();
    settings.maxMinorSteps = m_maxMinorSteps->value();
    settings.maxWeeks = m_maxWeeks->value();

    return settings;
}

#include "moc_Panel.cpp"
