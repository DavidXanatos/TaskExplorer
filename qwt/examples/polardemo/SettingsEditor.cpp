/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "SettingsEditor.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLayout>

SettingsEditor::SettingsEditor( QWidget* parent )
    : QFrame( parent )
{
    const int margin = 20;

    QGroupBox* axesBox = new QGroupBox( "Axes", this );
    QVBoxLayout* axesBoxLayout = new QVBoxLayout( axesBox );
    axesBoxLayout->setContentsMargins( margin, margin, margin, margin );

    for ( int i = PlotSettings::AxisBegin;
        i <= PlotSettings::Logarithmic; i++ )
    {
        m_checkBox[i] = new QCheckBox( axesBox );
        axesBoxLayout->addWidget( m_checkBox[i] );
    }

    QGroupBox* gridBox = new QGroupBox( "Grids", this );
    QVBoxLayout* gridBoxLayout = new QVBoxLayout( gridBox );
    gridBoxLayout->setContentsMargins( margin, margin, margin, margin );

    for ( int scaleId = 0; scaleId < QwtPolar::ScaleCount; scaleId++ )
    {
        int idx = PlotSettings::MajorGridBegin + scaleId;
        m_checkBox[idx] = new QCheckBox( gridBox );
        gridBoxLayout->addWidget( m_checkBox[idx] );

        idx = PlotSettings::MinorGridBegin + scaleId;
        m_checkBox[idx] = new QCheckBox( gridBox );
        gridBoxLayout->addWidget( m_checkBox[idx] );
    }
    gridBoxLayout->addStretch( 10 );

    QGroupBox* otherBox = new QGroupBox( "Other", this );
    QVBoxLayout* otherBoxLayout = new QVBoxLayout( otherBox );
    otherBoxLayout->setContentsMargins( margin, margin, margin, margin );

    for ( int i = PlotSettings::Logarithmic + 1;
        i < PlotSettings::NumFlags; i++ )
    {
        m_checkBox[i] = new QCheckBox( otherBox );
        otherBoxLayout->addWidget( m_checkBox[i] );
    }
    otherBoxLayout->addStretch( 10 );

    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->addWidget( axesBox );
    layout->addWidget( gridBox );
    layout->addWidget( otherBox );
    layout->addStretch( 10 );

    for ( int i = 0; i < PlotSettings::NumFlags; i++ )
    {
        m_checkBox[i]->setText( label( i ) );
        connect( m_checkBox[i], SIGNAL(clicked()), this, SLOT(edited()) );
    }
}

void SettingsEditor::showSettings( const PlotSettings& settings )
{
    blockSignals( true );
    for ( int i = 0; i < PlotSettings::NumFlags; i++ )
        m_checkBox[i]->setChecked( settings.flags[i] );

    blockSignals( false );
    updateEditor();
}

PlotSettings SettingsEditor::settings() const
{
    PlotSettings s;
    for ( int i = 0; i < PlotSettings::NumFlags; i++ )
        s.flags[i] = m_checkBox[i]->isChecked();
    return s;
}

void SettingsEditor::edited()
{
    updateEditor();
    Q_EMIT edited( settings() );
}

void SettingsEditor::updateEditor()
{
    for ( int scaleId = 0; scaleId < QwtPolar::ScaleCount; scaleId++ )
    {
        m_checkBox[PlotSettings::MinorGridBegin + scaleId]->setEnabled(
            m_checkBox[PlotSettings::MajorGridBegin + scaleId]->isChecked() );
    }
}

QString SettingsEditor::label( int flag ) const
{
    switch( flag )
    {
        case PlotSettings::MajorGridBegin + QwtPolar::ScaleAzimuth:
            return "Azimuth";

        case PlotSettings::MajorGridBegin + QwtPolar::ScaleRadius:
            return "Radius";

        case PlotSettings::MinorGridBegin + QwtPolar::ScaleAzimuth:
            return "Azimuth Minor";

        case PlotSettings::MinorGridBegin + QwtPolar::ScaleRadius:
            return "Radius Minor";

        case PlotSettings::AxisBegin + QwtPolar::AxisAzimuth:
            return "Azimuth";

        case PlotSettings::AxisBegin + QwtPolar::AxisLeft:
            return "Left";

        case PlotSettings::AxisBegin + QwtPolar::AxisRight:
            return "Right";

        case PlotSettings::AxisBegin + QwtPolar::AxisTop:
            return "Top";

        case PlotSettings::AxisBegin + QwtPolar::AxisBottom:
            return "Bottom";

        case PlotSettings::AutoScaling:
            return "Auto Scaling";

        case PlotSettings::Inverted:
            return "Inverted";

        case PlotSettings::Logarithmic:
            return "Logarithmic";

        case PlotSettings::Antialiasing:
            return "Antialiasing";

        case PlotSettings::CurveBegin + PlotSettings::Spiral:
            return "Spiral Curve";

        case PlotSettings::CurveBegin + PlotSettings::Rose:
            return "Rose Curve";
    }

    return QString();
}

#include "moc_SettingsEditor.cpp"
