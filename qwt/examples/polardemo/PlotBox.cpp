/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "PlotBox.h"
#include "Plot.h"
#include "SettingsEditor.h"

#include <QwtPolarPanner>
#include <QwtPolarMagnifier>
#include <QwtPolarRenderer>

#include <QPrinter>
#include <QPrintDialog>
#include <QHBoxLayout>

PlotBox::PlotBox( QWidget* parent )
    : QWidget( parent )
{
    m_plot = new Plot();

    m_panner = new QwtPolarPanner( m_plot->canvas() );
    m_panner->setEnabled( false );

    m_zoomer = new QwtPolarMagnifier( m_plot->canvas() );
    m_zoomer->setEnabled( false );

    SettingsEditor* settingsEditor = new SettingsEditor();
    settingsEditor->showSettings( m_plot->settings() );
    connect( settingsEditor, SIGNAL(edited(const PlotSettings&)),
        m_plot, SLOT(applySettings(const PlotSettings&)) );

    QHBoxLayout* layout = new QHBoxLayout( this );
    layout->addWidget( settingsEditor, 0 );
    layout->addWidget( m_plot, 10 );
}

void PlotBox::printDocument()
{
    QPrinter printer( QPrinter::HighResolution );

    QString docName = m_plot->title().text();
    if ( !docName.isEmpty() )
    {
        docName.replace ( "\n", " -- " );
        printer.setDocName ( docName );
    }

    printer.setCreator( "polar plot demo example" );
#if QT_VERSION >= 0x050300
    printer.setPageOrientation( QPageLayout::Landscape );
#else
    printer.setOrientation( QPrinter::Landscape );
#endif

    QPrintDialog dialog( &printer );
    if ( dialog.exec() )
    {
        QwtPolarRenderer renderer;
        renderer.renderTo( m_plot, printer );
    }
}

void PlotBox::exportDocument()
{
    QString fileName = "polarplot.pdf";

    QwtPolarRenderer renderer;
    renderer.exportTo( m_plot, "polarplot.pdf" );
}

void PlotBox::enableZoomMode( bool on )
{
    m_panner->setEnabled( on );
    m_zoomer->setEnabled( on );
}

#include "moc_PlotBox.cpp"
