/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Panel.h"

#include <QwtPlotCurve>

#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLayout>

class SpinBox : public QSpinBox
{
  public:
    SpinBox( int min, int max, int step, QWidget* parent )
        : QSpinBox( parent )
    {
        setRange( min, max );
        setSingleStep( step );
    }
};

class CheckBox : public QCheckBox
{
  public:
    CheckBox( const QString& title, QWidget* parent )
        : QCheckBox( title, parent )
    {
    }

    void setChecked( bool checked )
    {
        setCheckState( checked ? Qt::Checked : Qt::Unchecked );
    }

    bool isChecked() const
    {
        return checkState() == Qt::Checked;
    }
};

Panel::Panel( QWidget* parent )
    : QTabWidget( parent )
{
    setTabPosition( QTabWidget::West );

    addTab( createPlotTab( this ), "Plot" );
    addTab( createCanvasTab( this ), "Canvas" );
    addTab( createCurveTab( this ), "Curve" );

    setSettings( Settings() );

    connect( m_numPoints, SIGNAL(valueChanged(int)), SLOT(edited()) );
    connect( m_updateInterval, SIGNAL(valueChanged(int)), SLOT(edited()) );
    connect( m_curveWidth, SIGNAL(valueChanged(int)), SLOT(edited()) );

    connect( m_paintCache, SIGNAL(stateChanged(int)), SLOT(edited()) );
    connect( m_paintOnScreen, SIGNAL(stateChanged(int)), SLOT(edited()) );
    connect( m_immediatePaint, SIGNAL(stateChanged(int)), SLOT(edited()) );
#ifndef QWT_NO_OPENGL
    connect( m_openGL, SIGNAL(stateChanged(int)), SLOT(edited()) );
#endif

    connect( m_curveAntialiasing, SIGNAL(stateChanged(int)), SLOT(edited()) );
    connect( m_curveClipping, SIGNAL(stateChanged(int)), SLOT(edited()) );
    connect( m_curveWeeding, SIGNAL(currentIndexChanged(int)), SLOT(edited()) );
    connect( m_lineSplitting, SIGNAL(stateChanged(int)), SLOT(edited()) );
    connect( m_curveFilled, SIGNAL(stateChanged(int)), SLOT(edited()) );

    connect( m_updateType, SIGNAL(currentIndexChanged(int)), SLOT(edited()) );
    connect( m_gridStyle, SIGNAL(currentIndexChanged(int)), SLOT(edited()) );
    connect( m_curveType, SIGNAL(currentIndexChanged(int)), SLOT(edited()) );
    connect( m_curvePen, SIGNAL(currentIndexChanged(int)), SLOT(edited()) );
}

QWidget* Panel::createPlotTab( QWidget* parent )
{
    QWidget* page = new QWidget( parent );

    m_updateInterval = new SpinBox( 0, 1000, 10, page );
    m_numPoints = new SpinBox( 10, 1000000, 1000, page );

    m_updateType = new QComboBox( page );
    m_updateType->addItem( "Repaint" );
    m_updateType->addItem( "Replot" );

    int row = 0;

    QGridLayout* layout = new QGridLayout( page );

    layout->addWidget( new QLabel( "Updates", page ), row, 0 );
    layout->addWidget( m_updateInterval, row, 1 );
    layout->addWidget( new QLabel( "ms", page ), row++, 2 );

    layout->addWidget( new QLabel( "Points", page ), row, 0 );
    layout->addWidget( m_numPoints, row++, 1 );

    layout->addWidget( new QLabel( "Update", page ), row, 0 );
    layout->addWidget( m_updateType, row++, 1 );

    layout->addLayout( new QHBoxLayout(), row++, 0 );

    layout->setColumnStretch( 1, 10 );
    layout->setRowStretch( row, 10 );

    return page;
}

QWidget* Panel::createCanvasTab( QWidget* parent )
{
    QWidget* page = new QWidget( parent );

    m_gridStyle = new QComboBox( page );
    m_gridStyle->addItem( "None" );
    m_gridStyle->addItem( "Solid" );
    m_gridStyle->addItem( "Dashes" );

    m_paintCache = new CheckBox( "Paint Cache", page );
    m_paintOnScreen = new CheckBox( "Paint On Screen", page );
    m_immediatePaint = new CheckBox( "Immediate Paint", page );
#ifndef QWT_NO_OPENGL
    m_openGL = new CheckBox( "OpenGL", page );
#endif

    int row = 0;

    QGridLayout* layout = new QGridLayout( page );
    layout->addWidget( new QLabel( "Grid", page ), row, 0 );
    layout->addWidget( m_gridStyle, row++, 1 );

    layout->addWidget( m_paintCache, row++, 0, 1, -1 );
    layout->addWidget( m_paintOnScreen, row++, 0, 1, -1 );
    layout->addWidget( m_immediatePaint, row++, 0, 1, -1 );
#ifndef QWT_NO_OPENGL
    layout->addWidget( m_openGL, row++, 0, 1, -1 );
#endif

    layout->addLayout( new QHBoxLayout(), row++, 0 );

    layout->setColumnStretch( 1, 10 );
    layout->setRowStretch( row, 10 );

    return page;
}

QWidget* Panel::createCurveTab( QWidget* parent )
{
    QWidget* page = new QWidget( parent );

    m_curveType = new QComboBox( page );
    m_curveType->addItem( "Wave" );
    m_curveType->addItem( "Noise" );

    m_curveAntialiasing = new CheckBox( "Antialiasing", page );
    m_curveClipping = new CheckBox( "Clipping", page );
    m_lineSplitting = new CheckBox( "Split Lines", page );

    m_curveWeeding = new QComboBox( page );
    m_curveWeeding->addItem( "None" );
    m_curveWeeding->addItem( "Normal" );
    m_curveWeeding->addItem( "Aggressive" );

    m_curveWidth = new SpinBox( 0, 10, 1, page );

    m_curvePen = new QComboBox( page );
    m_curvePen->addItem( "Solid" );
    m_curvePen->addItem( "Dotted" );

    m_curveFilled = new CheckBox( "Filled", page );

    int row = 0;

    QGridLayout* layout = new QGridLayout( page );
    layout->addWidget( new QLabel( "Type", page ), row, 0 );
    layout->addWidget( m_curveType, row++, 1 );

    layout->addWidget( m_curveAntialiasing, row++, 0, 1, -1 );
    layout->addWidget( m_curveClipping, row++, 0, 1, -1 );
    layout->addWidget( m_lineSplitting, row++, 0, 1, -1 );

    layout->addWidget( new QLabel( "Weeding", page ), row, 0 );
    layout->addWidget( m_curveWeeding, row++, 1 );

    layout->addWidget( new QLabel( "Width", page ), row, 0 );
    layout->addWidget( m_curveWidth, row++, 1 );

    layout->addWidget( new QLabel( "Style", page ), row, 0 );
    layout->addWidget( m_curvePen, row++, 1 );

    layout->addWidget( m_curveFilled, row++, 0, 1, -1 );

    layout->addLayout( new QHBoxLayout(), row++, 0 );

    layout->setColumnStretch( 1, 10 );
    layout->setRowStretch( row, 10 );

    return page;
}

void Panel::edited()
{
    const Settings s = settings();
    Q_EMIT settingsChanged( s );
}


Settings Panel::settings() const
{
    Settings s;

    s.grid.pen = QPen( Qt::black, 0 );

    switch( m_gridStyle->currentIndex() )
    {
        case 0:
            s.grid.pen.setStyle( Qt::NoPen );
            break;
        case 2:
            s.grid.pen.setStyle( Qt::DashLine );
            break;
    }

    s.curve.pen.setStyle( m_curvePen->currentIndex() == 0 ?
        Qt::SolidLine : Qt::DotLine );
    s.curve.pen.setWidth( m_curveWidth->value() );
    s.curve.brush.setStyle( ( m_curveFilled->isChecked() ) ?
        Qt::SolidPattern : Qt::NoBrush );
    s.curve.numPoints = m_numPoints->value();
    s.curve.functionType = static_cast< Settings::FunctionType >(
        m_curveType->currentIndex() );

    if ( m_curveClipping->isChecked() )
        s.curve.paintAttributes |= QwtPlotCurve::ClipPolygons;
    else
        s.curve.paintAttributes &= ~QwtPlotCurve::ClipPolygons;

    s.curve.paintAttributes &= ~QwtPlotCurve::FilterPoints;
    s.curve.paintAttributes &= ~QwtPlotCurve::FilterPointsAggressive;

    switch( m_curveWeeding->currentIndex() )
    {
        case 1:
        {
            s.curve.paintAttributes |= QwtPlotCurve::FilterPoints;
            break;
        }
        case 2:
        {
            s.curve.paintAttributes |= QwtPlotCurve::FilterPointsAggressive;
            break;
        }
    }

    if ( m_curveAntialiasing->isChecked() )
        s.curve.renderHint |= QwtPlotItem::RenderAntialiased;
    else
        s.curve.renderHint &= ~QwtPlotItem::RenderAntialiased;

    s.curve.lineSplitting = ( m_lineSplitting->isChecked() );

    s.canvas.useBackingStore = ( m_paintCache->isChecked() );
    s.canvas.paintOnScreen = ( m_paintOnScreen->isChecked() );
    s.canvas.immediatePaint = ( m_immediatePaint->isChecked() );
#ifndef QWT_NO_OPENGL
    s.canvas.openGL = ( m_openGL->isChecked() );
#endif

    s.updateInterval = m_updateInterval->value();
    s.updateType = static_cast< Settings::UpdateType >( m_updateType->currentIndex() );

    return s;
}

void Panel::setSettings( const Settings& s )
{
    m_numPoints->setValue( s.curve.numPoints );
    m_updateInterval->setValue( s.updateInterval );
    m_updateType->setCurrentIndex( s.updateType );

    switch( s.grid.pen.style() )
    {
        case Qt::NoPen:
        {
            m_gridStyle->setCurrentIndex( 0 );
            break;
        }
        case Qt::DashLine:
        {
            m_gridStyle->setCurrentIndex( 2 );
            break;
        }
        default:
        {
            m_gridStyle->setCurrentIndex( 1 ); // Solid
        }
    }

    m_paintCache->setChecked( s.canvas.useBackingStore );
    m_paintOnScreen->setChecked( s.canvas.paintOnScreen );
    m_immediatePaint->setChecked( s.canvas.immediatePaint );
#ifndef QWT_NO_OPENGL
    m_openGL->setChecked( s.canvas.openGL );
#endif

    m_curveType->setCurrentIndex( s.curve.functionType );
    m_curveAntialiasing->setChecked(
        s.curve.renderHint & QwtPlotCurve::RenderAntialiased );

    m_curveClipping->setChecked(
        s.curve.paintAttributes & QwtPlotCurve::ClipPolygons );

    int weedingIndex = 0;
    if ( s.curve.paintAttributes & QwtPlotCurve::FilterPointsAggressive )
        weedingIndex = 2;
    else if ( s.curve.paintAttributes & QwtPlotCurve::FilterPoints )
        weedingIndex = 1;

    m_curveWeeding->setCurrentIndex( weedingIndex );

    m_lineSplitting->setChecked( s.curve.lineSplitting );

    m_curveWidth->setValue( s.curve.pen.width() );
    m_curvePen->setCurrentIndex(
        s.curve.pen.style() == Qt::SolidLine ? 0 : 1 );
    m_curveFilled->setChecked( s.curve.brush.style() != Qt::NoBrush );
}

#include "moc_Panel.cpp"
