/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "PlotWindow.h"

#include <QwtScaleEngine>
#include <QwtRasterData>
#include <QwtPolarSpectrogram>
#include <QwtScaleWidget>
#include <QwtColorMap>

#include <QLayout>

namespace
{
    class ColorMap : public QwtLinearColorMap
    {
      public:
        ColorMap()
            : QwtLinearColorMap( Qt::darkBlue, Qt::yellow )
        {
#if 1
            addColorStop( 0.00, Qt::black );
            addColorStop( 0.05, Qt::darkRed );
#else
            addColorStop( 0.05, Qt::blue );
#endif
            addColorStop( 0.3, Qt::cyan );
            addColorStop( 0.6, Qt::green );
            addColorStop( 0.98, Qt::red );
        }
    };
}

PlotWindow::PlotWindow( QWidget* parent )
    : QWidget( parent )
{
    m_plot = new Plot();

    m_colorScale = new QwtScaleWidget();
    m_colorScale->setAlignment( QwtScaleDraw::RightScale );
    m_colorScale->setColorBarEnabled( true );

    QwtText title( "Intensity" );
    QFont font = m_colorScale->font();
    font.setBold( true );
    title.setFont( font );
    m_colorScale->setTitle( title );

    const QwtInterval interval =
        m_plot->spectrogram()->data()->interval( Qt::ZAxis );

    m_colorScale->setColorMap( interval, new ColorMap() );
    m_plot->spectrogram()->setColorMap( new ColorMap() );

    QwtLinearScaleEngine scaleEngine;
    m_colorScale->setScaleDiv(
        scaleEngine.divideScale( interval.minValue(), interval.maxValue(), 8, 5 ) );

    int startDist, endDist;
    m_colorScale->getBorderDistHint( startDist, endDist );
    m_colorScale->setBorderDist( startDist, endDist );

    QHBoxLayout* layout = new QHBoxLayout( this );
    layout->addWidget( m_plot, 10 );
    layout->addWidget( m_colorScale, 10 );
}
