/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Panel.h"
#include "Settings.h"

#include <QwtPlot>
#include <QwtPlotLegendItem>

#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>

Panel::Panel( QWidget* parent )
    : QWidget( parent )
{
    // create widgets

    m_legend.checkBox = new QCheckBox( "Enabled" );

    m_legend.positionBox = new QComboBox();
    m_legend.positionBox->addItem( "Left", QwtPlot::LeftLegend );
    m_legend.positionBox->addItem( "Right", QwtPlot::RightLegend );
    m_legend.positionBox->addItem( "Bottom", QwtPlot::BottomLegend );
    m_legend.positionBox->addItem( "Top", QwtPlot::TopLegend );
    m_legend.positionBox->addItem( "External", QwtPlot::TopLegend + 1 );

    m_legendItem.checkBox = new QCheckBox( "Enabled" );

    m_legendItem.numColumnsBox = new QSpinBox();
    m_legendItem.numColumnsBox->setRange( 0, 10 );
    m_legendItem.numColumnsBox->setSpecialValueText( "Unlimited" );

    m_legendItem.hAlignmentBox = new QComboBox();
    m_legendItem.hAlignmentBox->addItem( "Left", Qt::AlignLeft );
    m_legendItem.hAlignmentBox->addItem( "Centered", Qt::AlignHCenter );
    m_legendItem.hAlignmentBox->addItem( "Right", Qt::AlignRight );

    m_legendItem.vAlignmentBox = new QComboBox();
    m_legendItem.vAlignmentBox->addItem( "Top", Qt::AlignTop );
    m_legendItem.vAlignmentBox->addItem( "Centered", Qt::AlignVCenter );
    m_legendItem.vAlignmentBox->addItem( "Bottom", Qt::AlignBottom );

    m_legendItem.backgroundBox = new QComboBox();
    m_legendItem.backgroundBox->addItem( "Legend",
        QwtPlotLegendItem::LegendBackground );
    m_legendItem.backgroundBox->addItem( "Items",
        QwtPlotLegendItem::ItemBackground );

    m_legendItem.sizeBox = new QSpinBox();
    m_legendItem.sizeBox->setRange( 8, 22 );

    m_curve.numCurves = new QSpinBox();
    m_curve.numCurves->setRange( 0, 99 );

    m_curve.title = new QLineEdit();

    // layout

    QGroupBox* legendBox = new QGroupBox( "Legend" );
    QGridLayout* legendBoxLayout = new QGridLayout( legendBox );

    int row = 0;
    legendBoxLayout->addWidget( m_legend.checkBox, row, 0, 1, -1 );

    row++;
    legendBoxLayout->addWidget( new QLabel( "Position" ), row, 0 );
    legendBoxLayout->addWidget( m_legend.positionBox, row, 1 );


    QGroupBox* legendItemBox = new QGroupBox( "Legend Item" );
    QGridLayout* legendItemBoxLayout = new QGridLayout( legendItemBox );

    row = 0;
    legendItemBoxLayout->addWidget( m_legendItem.checkBox, row, 0, 1, -1 );

    row++;
    legendItemBoxLayout->addWidget( new QLabel( "Columns" ), row, 0 );
    legendItemBoxLayout->addWidget( m_legendItem.numColumnsBox, row, 1 );

    row++;
    legendItemBoxLayout->addWidget( new QLabel( "Horizontal" ), row, 0 );
    legendItemBoxLayout->addWidget( m_legendItem.hAlignmentBox, row, 1 );

    row++;
    legendItemBoxLayout->addWidget( new QLabel( "Vertical" ), row, 0 );
    legendItemBoxLayout->addWidget( m_legendItem.vAlignmentBox, row, 1 );

    row++;
    legendItemBoxLayout->addWidget( new QLabel( "Background" ), row, 0 );
    legendItemBoxLayout->addWidget( m_legendItem.backgroundBox, row, 1 );

    row++;
    legendItemBoxLayout->addWidget( new QLabel( "Size" ), row, 0 );
    legendItemBoxLayout->addWidget( m_legendItem.sizeBox, row, 1 );

    QGroupBox* curveBox = new QGroupBox( "Curves" );
    QGridLayout* curveBoxLayout = new QGridLayout( curveBox );

    row = 0;
    curveBoxLayout->addWidget( new QLabel( "Number" ), row, 0 );
    curveBoxLayout->addWidget( m_curve.numCurves, row, 1 );

    row++;
    curveBoxLayout->addWidget( new QLabel( "Title" ), row, 0 );
    curveBoxLayout->addWidget( m_curve.title, row, 1 );

    QVBoxLayout* layout = new QVBoxLayout( this );
    layout->addWidget( legendBox );
    layout->addWidget( legendItemBox );
    layout->addWidget( curveBox );
    layout->addStretch( 10 );

    connect( m_legend.checkBox,
        SIGNAL(stateChanged(int)), SIGNAL(edited()) );
    connect( m_legend.positionBox,
        SIGNAL(currentIndexChanged(int)), SIGNAL(edited()) );

    connect( m_legendItem.checkBox,
        SIGNAL(stateChanged(int)), SIGNAL(edited()) );
    connect( m_legendItem.numColumnsBox,
        SIGNAL(valueChanged(int)), SIGNAL(edited()) );
    connect( m_legendItem.hAlignmentBox,
        SIGNAL(currentIndexChanged(int)), SIGNAL(edited()) );
    connect( m_legendItem.vAlignmentBox,
        SIGNAL(currentIndexChanged(int)), SIGNAL(edited()) );
    connect( m_legendItem.backgroundBox,
        SIGNAL(currentIndexChanged(int)), SIGNAL(edited()) );
    connect( m_curve.numCurves,
        SIGNAL(valueChanged(int)), SIGNAL(edited()) );
    connect( m_legendItem.sizeBox,
        SIGNAL(valueChanged(int)), SIGNAL(edited()) );
    connect( m_curve.title,
        SIGNAL(textEdited(const QString&)), SIGNAL(edited()) );
}

void Panel::setSettings( const Settings& settings)
{
    blockSignals( true );

    m_legend.checkBox->setCheckState(
        settings.legend.isEnabled ? Qt::Checked : Qt::Unchecked );
    m_legend.positionBox->setCurrentIndex( settings.legend.position );

    m_legendItem.checkBox->setCheckState(
        settings.legendItem.isEnabled ? Qt::Checked : Qt::Unchecked );

    m_legendItem.numColumnsBox->setValue( settings.legendItem.numColumns );

    int align = settings.legendItem.alignment;

    if ( align & Qt::AlignLeft )
        m_legendItem.hAlignmentBox->setCurrentIndex( 0 );
    else if ( align & Qt::AlignRight )
        m_legendItem.hAlignmentBox->setCurrentIndex( 2 );
    else
        m_legendItem.hAlignmentBox->setCurrentIndex( 1 );

    if ( align & Qt::AlignTop )
        m_legendItem.vAlignmentBox->setCurrentIndex( 0 );
    else if ( align & Qt::AlignBottom )
        m_legendItem.vAlignmentBox->setCurrentIndex( 2 );
    else
        m_legendItem.vAlignmentBox->setCurrentIndex( 1 );

    m_legendItem.backgroundBox->setCurrentIndex(
        settings.legendItem.backgroundMode );

    m_legendItem.sizeBox->setValue( settings.legendItem.size );

    m_curve.numCurves->setValue( settings.curve.numCurves );
    m_curve.title->setText( settings.curve.title );

    blockSignals( false );
}

Settings Panel::settings() const
{
    Settings s;

    s.legend.isEnabled =
        m_legend.checkBox->checkState() == Qt::Checked;
    s.legend.position = m_legend.positionBox->currentIndex();

    s.legendItem.isEnabled =
        m_legendItem.checkBox->checkState() == Qt::Checked;
    s.legendItem.numColumns = m_legendItem.numColumnsBox->value();

    int align = 0;

    int hIndex = m_legendItem.hAlignmentBox->currentIndex();
    if ( hIndex == 0 )
        align |= Qt::AlignLeft;
    else if ( hIndex == 2 )
        align |= Qt::AlignRight;
    else
        align |= Qt::AlignHCenter;

    int vIndex = m_legendItem.vAlignmentBox->currentIndex();
    if ( vIndex == 0 )
        align |= Qt::AlignTop;
    else if ( vIndex == 2 )
        align |= Qt::AlignBottom;
    else
        align |= Qt::AlignVCenter;

    s.legendItem.alignment = align;

    s.legendItem.backgroundMode =
        m_legendItem.backgroundBox->currentIndex();
    s.legendItem.size = m_legendItem.sizeBox->value();

    s.curve.numCurves = m_curve.numCurves->value();
    s.curve.title = m_curve.title->text();

    return s;
}

#include "moc_Panel.cpp"
