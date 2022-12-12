/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "PlotMatrix.h"

#include <QwtPlot>
#include <QwtPlotCanvas>
#include <QwtScaleWidget>
#include <QwtScaleDraw>

#include <QLayout>

static void enablePlotAxis( QwtPlot* plot, int axis, bool on )
{
    // when false we still enable the axis to have an effect
    // of the minimal extent active. Instead we hide all visible
    // parts and margins/spacings.

    plot->setAxisVisible( axis, true );

    QwtScaleDraw* sd = plot->axisScaleDraw( axis );
    sd->enableComponent( QwtScaleDraw::Backbone, on );
    sd->enableComponent( QwtScaleDraw::Ticks, on );
    sd->enableComponent( QwtScaleDraw::Labels, on );

    QwtScaleWidget* sw = plot->axisWidget( axis );
    sw->setMargin( on ? 4 : 0 );
    sw->setSpacing( on ? 20 : 0 );
}

namespace
{
    class Plot : public QwtPlot
    {
      public:
        Plot( QWidget* parent = NULL )
            : QwtPlot( parent )
        {
            QwtPlotCanvas* canvas = new QwtPlotCanvas();
            canvas->setLineWidth( 1 );
            canvas->setFrameStyle( QFrame::Box | QFrame::Plain );

            setCanvas( canvas );
        }

        virtual QSize sizeHint() const QWT_OVERRIDE
        {
            return minimumSizeHint();
        }
    };
}

class PlotMatrix::PrivateData
{
  public:
    PrivateData():
        inScaleSync( false )
    {
        isAxisEnabled[QwtAxis::XBottom] = true;
        isAxisEnabled[QwtAxis::XTop] = false;
        isAxisEnabled[QwtAxis::YLeft] = true;
        isAxisEnabled[QwtAxis::YRight] = false;
    }

    bool isAxisEnabled[QwtAxis::AxisPositions];
    QVector< QwtPlot* > plotWidgets;
    mutable bool inScaleSync;
};

PlotMatrix::PlotMatrix( int numRows, int numColumns, QWidget* parent )
    : QFrame( parent )
{
    m_data = new PrivateData();
    m_data->plotWidgets.resize( numRows * numColumns );

    QGridLayout* layout = new QGridLayout( this );
    layout->setSpacing( 5 );

    for ( int row = 0; row < numRows; row++ )
    {
        for ( int col = 0; col < numColumns; col++ )
        {
            QwtPlot* plot = new Plot( this );

            layout->addWidget( plot, row, col );

            for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
            {
                connect( plot->axisWidget( axisPos ),
                    SIGNAL(scaleDivChanged()), SLOT(scaleDivChanged()),
                    Qt::QueuedConnection );
            }
            m_data->plotWidgets[row * numColumns + col] = plot;
        }
    }

    updateLayout();
}

PlotMatrix::~PlotMatrix()
{
    delete m_data;
}

int PlotMatrix::numRows() const
{
    const QGridLayout* l = qobject_cast< const QGridLayout* >( layout() );
    if ( l )
        return l->rowCount();

    return 0;
}

int PlotMatrix::numColumns() const
{
    const QGridLayout* l = qobject_cast< const QGridLayout* >( layout() );
    if ( l )
        return l->columnCount();
    return 0;
}

QwtPlot* PlotMatrix::plotAt( int row, int column )
{
    const int index = row * numColumns() + column;
    if ( index < m_data->plotWidgets.size() )
        return m_data->plotWidgets[index];

    return NULL;
}

const QwtPlot* PlotMatrix::plotAt( int row, int column ) const
{
    const int index = row * numColumns() + column;
    if ( index < m_data->plotWidgets.size() )
        return m_data->plotWidgets[index];

    return NULL;
}

void PlotMatrix::setAxisVisible( int axis, bool tf )
{
    if ( QwtAxis::isValid( axis ) )
    {
        if ( tf != m_data->isAxisEnabled[axis] )
        {
            m_data->isAxisEnabled[axis] = tf;
            updateLayout();
        }
    }
}

bool PlotMatrix::isAxisVisible( int axis ) const
{
    if ( QwtAxis::isValid( axis ) )
        return m_data->isAxisEnabled[axis];

    return false;
}

void PlotMatrix::setAxisScale( int axis, int rowOrColumn,
    double min, double max, double step )
{
    int row = 0;
    int col = 0;

    if ( QwtAxis::isXAxis( axis ) )
        col = rowOrColumn;
    else
        row = rowOrColumn;

    QwtPlot* plt = plotAt( row, col );
    if ( plt )
    {
        plt->setAxisScale( axis, min, max, step );
        plt->updateAxes();
    }
}

void PlotMatrix::scaleDivChanged()
{
    if ( m_data->inScaleSync )
        return;

    m_data->inScaleSync = true;

    QwtPlot* plt = NULL;
    int axisId = -1;
    int rowOrColumn = -1;

    // find the changed axis
    for ( int row = 0; row < numRows(); row++ )
    {
        for ( int col = 0; col < numColumns(); col++ )
        {
            QwtPlot* p = plotAt( row, col );
            if ( p )
            {
                for ( int axisPos = 0; axisPos < QwtAxis::AxisPositions; axisPos++ )
                {
                    if ( p->axisWidget( axisPos ) == sender() )
                    {
                        plt = p;
                        axisId = axisPos;
                        rowOrColumn = QwtAxis::isXAxis( axisId ) ? col : row;
                    }
                }
            }
        }
    }

    if ( plt )
    {
        const QwtScaleDiv scaleDiv = plt->axisScaleDiv( axisId );

        // synchronize the axes
        if ( QwtAxis::isXAxis( axisId ) )
        {
            for ( int row = 0; row < numRows(); row++ )
            {
                QwtPlot* p = plotAt( row, rowOrColumn );
                if ( p != plt )
                {
                    p->setAxisScaleDiv( axisId, scaleDiv );
                }
            }
        }
        else
        {
            for ( int col = 0; col < numColumns(); col++ )
            {
                QwtPlot* p = plotAt( rowOrColumn, col );
                if ( p != plt )
                {
                    p->setAxisScaleDiv( axisId, scaleDiv );
                }
            }
        }

        updateLayout();
    }

    m_data->inScaleSync = false;
}

void PlotMatrix::updateLayout()
{
    using namespace QwtAxis;

    for ( int row = 0; row < numRows(); row++ )
    {
        for ( int col = 0; col < numColumns(); col++ )
        {
            QwtPlot* p = plotAt( row, col );
            if ( p )
            {
                bool showAxis[AxisPositions];

                showAxis[XBottom] = isAxisVisible( XBottom ) && row == numRows() - 1;
                showAxis[XTop] = isAxisVisible( XTop ) && row == 0;
                showAxis[YLeft] = isAxisVisible( YLeft ) && col == 0;
                showAxis[YRight] = isAxisVisible( YRight ) && col == numColumns() - 1;

                for ( int axis = 0; axis < AxisPositions; axis++ )
                {
                    enablePlotAxis( p, axis, showAxis[axis] );
                }
            }
        }
    }

    for ( int row = 0; row < numRows(); row++ )
    {
        alignAxes( row, XTop );
        alignAxes( row, XBottom );

        alignScaleBorder( row, YLeft );
        alignScaleBorder( row, YRight );
    }

    for ( int col = 0; col < numColumns(); col++ )
    {
        alignAxes( col, YLeft );
        alignAxes( col, YRight );

        alignScaleBorder( col, XBottom );
        alignScaleBorder( col, XTop );
    }

    for ( int row = 0; row < numRows(); row++ )
    {
        for ( int col = 0; col < numColumns(); col++ )
        {
            QwtPlot* p = plotAt( row, col );
            if ( p )
                p->replot();
        }
    }
}

void PlotMatrix::alignAxes( int rowOrColumn, int axis )
{
    if ( QwtAxis::isYAxis( axis ) )
    {
        double maxExtent = 0;

        for ( int row = 0; row < numRows(); row++ )
        {
            QwtPlot* p = plotAt( row, rowOrColumn );
            if ( p )
            {
                QwtScaleWidget* scaleWidget = p->axisWidget( axis );

                QwtScaleDraw* sd = scaleWidget->scaleDraw();
                sd->setMinimumExtent( 0.0 );

                const double extent = sd->extent( scaleWidget->font() );
                if ( extent > maxExtent )
                    maxExtent = extent;
            }
        }

        for ( int row = 0; row < numRows(); row++ )
        {
            QwtPlot* p = plotAt( row, rowOrColumn );
            if ( p )
            {
                QwtScaleWidget* scaleWidget = p->axisWidget( axis );
                scaleWidget->scaleDraw()->setMinimumExtent( maxExtent );
            }
        }
    }
    else
    {
        double maxExtent = 0;

        for ( int col = 0; col < numColumns(); col++ )
        {
            QwtPlot* p = plotAt( rowOrColumn, col );
            if ( p )
            {
                QwtScaleWidget* scaleWidget = p->axisWidget( axis );

                QwtScaleDraw* sd = scaleWidget->scaleDraw();
                sd->setMinimumExtent( 0.0 );

                const double extent = sd->extent( scaleWidget->font() );
                if ( extent > maxExtent )
                    maxExtent = extent;
            }
        }

        for ( int col = 0; col < numColumns(); col++ )
        {
            QwtPlot* p = plotAt( rowOrColumn, col );
            if ( p )
            {
                QwtScaleWidget* scaleWidget = p->axisWidget( axis );
                scaleWidget->scaleDraw()->setMinimumExtent( maxExtent );
            }
        }
    }
}

void PlotMatrix::alignScaleBorder( int rowOrColumn, int axis )
{
    int startDist = 0;
    int endDist = 0;

    if ( axis == QwtAxis::YLeft )
    {
        QwtPlot* plot = plotAt( rowOrColumn, 0 );
        if ( plot )
            plot->axisWidget( axis )->getBorderDistHint( startDist, endDist );

        for ( int col = 1; col < numColumns(); col++ )
        {
            plot = plotAt( rowOrColumn, col );
            if ( plot )
                plot->axisWidget( axis )->setMinBorderDist( startDist, endDist );
        }
    }
    else if ( axis == QwtAxis::YRight )
    {
        QwtPlot* plot = plotAt( rowOrColumn, numColumns() - 1 );
        if ( plot )
            plot->axisWidget( axis )->getBorderDistHint( startDist, endDist );

        for ( int col = 0; col < numColumns() - 1; col++ )
        {
            plot = plotAt( rowOrColumn, col );
            if ( plot )
                plot->axisWidget( axis )->setMinBorderDist( startDist, endDist );
        }
    }

    if ( axis == QwtAxis::XTop )
    {
        QwtPlot* plot = plotAt( rowOrColumn, 0 );
        if ( plot )
            plot->axisWidget( axis )->getBorderDistHint( startDist, endDist );

        for ( int row = 1; row < numRows(); row++ )
        {
            plot = plotAt( row, rowOrColumn );
            if ( plot )
                plot->axisWidget( axis )->setMinBorderDist( startDist, endDist );
        }
    }
    else if ( axis == QwtAxis::XBottom )
    {
        QwtPlot* plot = plotAt( numRows() - 1, rowOrColumn );
        if ( plot )
            plot->axisWidget( axis )->getBorderDistHint( startDist, endDist );

        for ( int row = 0; row < numRows() - 1; row++ )
        {
            plot = plotAt( row, rowOrColumn );
            if ( plot )
                plot->axisWidget( axis )->setMinBorderDist( startDist, endDist );
        }
    }
}

#include "moc_PlotMatrix.cpp"
