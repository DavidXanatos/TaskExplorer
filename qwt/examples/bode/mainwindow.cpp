/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "MainWindow.h"
#include "Plot.h"
#include "Pixmaps.h"

#include <QwtCounter>
#include <QwtPickerMachine>
#include <QwtPlotZoomer>
#include <QwtPlotPanner>
#include <QwtPlotRenderer>
#include <QwtText>

#include <QToolBar>
#include <QToolButton>
#include <QLabel>
#include <QLayout>
#include <QStatusBar>
#include <QPrinter>
#include <QPrintDialog>
#include <QPen>

namespace
{
    class Zoomer : public QwtPlotZoomer
    {
      public:
        Zoomer( int xAxis, int yAxis, QWidget* canvas )
            : QwtPlotZoomer( xAxis, yAxis, canvas )
        {
            setTrackerMode( QwtPicker::AlwaysOff );
            setRubberBand( QwtPicker::NoRubberBand );

            // RightButton: zoom out by 1
            // Ctrl+RightButton: zoom out to full size

            setMousePattern( QwtEventPattern::MouseSelect2,
                Qt::RightButton, Qt::ControlModifier );
            setMousePattern( QwtEventPattern::MouseSelect3,
                Qt::RightButton );
        }
    };
}

MainWindow::MainWindow( QWidget* parent )
    : QMainWindow( parent )
{
    m_plot = new Plot( this );

    const int margin = 5;
    m_plot->setContentsMargins( margin, margin, margin, 0 );

    setContextMenuPolicy( Qt::NoContextMenu );

    m_zoomer[0] = new Zoomer( QwtAxis::XBottom, QwtAxis::YLeft,
        m_plot->canvas() );
    m_zoomer[0]->setRubberBand( QwtPicker::RectRubberBand );
    m_zoomer[0]->setRubberBandPen( QColor( Qt::green ) );
    m_zoomer[0]->setTrackerMode( QwtPicker::ActiveOnly );
    m_zoomer[0]->setTrackerPen( QColor( Qt::white ) );

    m_zoomer[1] = new Zoomer(
        QwtAxis::XTop, QwtAxis::YRight, m_plot->canvas() );

    m_panner = new QwtPlotPanner( m_plot->canvas() );
    m_panner->setMouseButton( Qt::MiddleButton );

    m_picker = new QwtPlotPicker( QwtAxis::XBottom, QwtAxis::YLeft,
        QwtPlotPicker::CrossRubberBand, QwtPicker::AlwaysOn,
        m_plot->canvas() );
    m_picker->setStateMachine( new QwtPickerDragPointMachine() );
    m_picker->setRubberBandPen( QColor( Qt::green ) );
    m_picker->setRubberBand( QwtPicker::CrossRubberBand );
    m_picker->setTrackerPen( QColor( Qt::white ) );

    setCentralWidget( m_plot );

    QToolBar* toolBar = new QToolBar( this );

    QToolButton* btnZoom = new QToolButton( toolBar );
    btnZoom->setText( "Zoom" );
    btnZoom->setIcon( QPixmap( zoom_xpm ) );
    btnZoom->setCheckable( true );
    btnZoom->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    toolBar->addWidget( btnZoom );
    connect( btnZoom, SIGNAL(toggled(bool)), SLOT(enableZoomMode(bool)) );

#ifndef QT_NO_PRINTER
    QToolButton* btnPrint = new QToolButton( toolBar );
    btnPrint->setText( "Print" );
    btnPrint->setIcon( QPixmap( print_xpm ) );
    btnPrint->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    toolBar->addWidget( btnPrint );
    connect( btnPrint, SIGNAL(clicked()), SLOT(print()) );
#endif

    QToolButton* btnExport = new QToolButton( toolBar );
    btnExport->setText( "Export" );
    btnExport->setIcon( QPixmap( print_xpm ) );
    btnExport->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    toolBar->addWidget( btnExport );
    connect( btnExport, SIGNAL(clicked()), SLOT(exportDocument()) );

    toolBar->addSeparator();

    QWidget* hBox = new QWidget( toolBar );

    QHBoxLayout* layout = new QHBoxLayout( hBox );
    layout->setSpacing( 0 );
    layout->addWidget( new QWidget( hBox ), 10 ); // spacer
    layout->addWidget( new QLabel( "Damping Factor", hBox ), 0 );
    layout->addSpacing( 10 );

    QwtCounter* cntDamp = new QwtCounter( hBox );
    cntDamp->setRange( 0.0, 5.0 );
    cntDamp->setSingleStep( 0.01 );
    cntDamp->setValue( 0.0 );

    layout->addWidget( cntDamp, 0 );

    ( void )toolBar->addWidget( hBox );

    addToolBar( toolBar );
#ifndef QT_NO_STATUSBAR
    ( void )statusBar();
#endif

    enableZoomMode( false );
    showInfo();

    connect( cntDamp, SIGNAL(valueChanged(double)),
        m_plot, SLOT(setDamp(double)) );

    connect( m_picker, SIGNAL(moved(const QPoint&)),
        SLOT(moved(const QPoint&)) );
    connect( m_picker, SIGNAL(selected(const QPolygon&)),
        SLOT(selected(const QPolygon&)) );
}

#ifndef QT_NO_PRINTER

void MainWindow::print()
{
    QPrinter printer( QPrinter::HighResolution );

    QString docName = m_plot->title().text();
    if ( !docName.isEmpty() )
    {
        docName.replace ( "\n", " -- " );
        printer.setDocName ( docName );
    }

    printer.setCreator( "Bode example" );
#if QT_VERSION >= 0x050300
    printer.setPageOrientation( QPageLayout::Landscape );
#else
    printer.setOrientation( QPrinter::Landscape );
#endif

    QPrintDialog dialog( &printer );
    if ( dialog.exec() )
    {
        QwtPlotRenderer renderer;

        if ( printer.colorMode() == QPrinter::GrayScale )
        {
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardBackground );
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardCanvasBackground );
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardCanvasFrame );
            renderer.setLayoutFlag( QwtPlotRenderer::FrameWithScales );
        }

        renderer.renderTo( m_plot, printer );
    }
}

#endif

void MainWindow::exportDocument()
{
    QwtPlotRenderer renderer;
    renderer.exportTo( m_plot, "bode.pdf" );
}

void MainWindow::enableZoomMode( bool on )
{
    m_panner->setEnabled( on );

    m_zoomer[0]->setEnabled( on );
    m_zoomer[0]->zoom( 0 );

    m_zoomer[1]->setEnabled( on );
    m_zoomer[1]->zoom( 0 );

    m_picker->setEnabled( !on );

    showInfo();
}

void MainWindow::showInfo( QString text )
{
    if ( text.isEmpty() )
    {
        if ( m_picker->rubberBand() )
            text = "Cursor Pos: Press left mouse button in plot region";
        else
            text = "Zoom: Press mouse button and drag";
    }

#ifndef QT_NO_STATUSBAR
    statusBar()->showMessage( text );
#endif
}

void MainWindow::moved( const QPoint& pos )
{
    using namespace QwtAxis;

    QString info( "Freq=%1, Ampl=%2, Phase=%3" );
    info = info.arg( m_plot->invTransform( XBottom, pos.x() ) );
    info = info.arg( m_plot->invTransform( YLeft, pos.y() ) );
    info = info.arg( m_plot->invTransform( YRight, pos.y() ) );

    showInfo( info );
}

void MainWindow::selected( const QPolygon& )
{
    showInfo();
}

#include "moc_MainWindow.cpp"
