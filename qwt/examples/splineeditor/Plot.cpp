/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "Plot.h"
#include "ScalePicker.h"
#include "CanvasPicker.h"

#include <QwtPlotLayout>
#include <QwtPlotCurve>
#include <QwtPlotMarker>
#include <QwtPlotRenderer>
#include <QwtSymbol>
#include <QwtScaleWidget>
#include <QwtWheel>
#include <QwtSplineLocal>
#include <QwtSplineCubic>
#include <QwtSplinePleasing>
#include <QwtSplineBasis>
#include <QwtSplineParametrization>
#include <QwtCurveFitter>
#include <QwtLegend>
#include <QwtLegendLabel>

#include <QEvent>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainterPath>

namespace
{
    class Symbol : public QwtSymbol
    {
      public:
        Symbol()
            : QwtSymbol( QwtSymbol::Ellipse )
        {
            QColor c( Qt::gray );
            c.setAlpha( 100 );
            setBrush( c );

            setPen( Qt::black );
            setSize( 8 );
        }
    };

    class SplineFitter : public QwtCurveFitter
    {
      public:
        enum Mode
        {
            PChipSpline,
            AkimaSpline,
            CubicSpline,
            CardinalSpline,
            ParabolicBlendingSpline,
            PleasingSpline,
            BasisSpline
        };

        SplineFitter( Mode mode )
            : QwtCurveFitter( QwtCurveFitter::Path )
            , m_spline(NULL)
        {
            switch( mode )
            {
                case PleasingSpline:
                {
                    m_spline = new QwtSplinePleasing();
                    break;
                }
                case PChipSpline:
                {
                    m_spline = new QwtSplineLocal( QwtSplineLocal::PChip );
                    break;
                }
                case AkimaSpline:
                {
                    m_spline = new QwtSplineLocal( QwtSplineLocal::Akima );
                    break;
                }
                case CubicSpline:
                {
                    m_spline = new QwtSplineCubic();
                    break;
                }
                case CardinalSpline:
                {
                    m_spline = new QwtSplineLocal( QwtSplineLocal::Cardinal );
                    break;
                }
                case ParabolicBlendingSpline:
                {
                    m_spline = new QwtSplineLocal( QwtSplineLocal::ParabolicBlending );
                    break;
                }
                case BasisSpline:
                {
                    m_spline = new QwtSplineBasis();
                    break;
                }
            }
            if ( m_spline )
                m_spline->setParametrization( QwtSplineParametrization::ParameterX );
        }

        ~SplineFitter()
        {
            delete m_spline;
        }

        void setClosing( bool on )
        {
            if ( m_spline == NULL )
                return;

            m_spline->setBoundaryType(
                on ? QwtSpline::ClosedPolygon : QwtSpline::ConditionalBoundaries );
        }

        void setBoundaryCondition( const QString& condition )
        {
            QwtSplineC2* splineC2 = dynamic_cast< QwtSplineC2* >( m_spline );
            if ( splineC2 )
            {
                if ( condition == "Cubic Runout" )
                {
                    setBoundaryConditions( QwtSplineC2::CubicRunout );
                    return;
                }

                if ( condition == "Not a Knot" )
                {
                    setBoundaryConditions( QwtSplineC2::NotAKnot );
                    return;
                }
            }

            QwtSplineC1* splineC1 = dynamic_cast< QwtSplineC1* >( m_spline );
            if ( splineC1 )
            {
                if ( condition == "Linear Runout" )
                {
                    setBoundaryConditions( QwtSpline::LinearRunout, 0.0 );
                    return;
                }

                if ( condition == "Parabolic Runout" )
                {
                    // Parabolic Runout means clamping the 3rd derivative to 0.0
                    setBoundaryConditions( QwtSpline::Clamped3, 0.0 );
                    return;
                }
            }

            // Natural
            setBoundaryConditions( QwtSplineC1::Clamped2, 0.0 );
        }

        void setParametric( const QString& parameterType )
        {
            QwtSplineParametrization::Type type = QwtSplineParametrization::ParameterX;

            if ( parameterType == "Uniform" )
            {
                type = QwtSplineParametrization::ParameterUniform;
            }
            else if ( parameterType == "Centripetral" )
            {
                type = QwtSplineParametrization::ParameterCentripetal;
            }
            else if ( parameterType == "Chordal" )
            {
                type = QwtSplineParametrization::ParameterChordal;
            }
            else if ( parameterType == "Manhattan" )
            {
                type = QwtSplineParametrization::ParameterManhattan;
            }

            m_spline->setParametrization( type );
        }

        virtual QPolygonF fitCurve( const QPolygonF& points ) const QWT_OVERRIDE
        {
            return m_spline->polygon( points, 0.5 );
        }

        virtual QPainterPath fitCurvePath( const QPolygonF& points ) const QWT_OVERRIDE
        {
            return m_spline->painterPath( points );
        }

      private:
        void setBoundaryConditions( int condition, double value = 0.0 )
        {
            if ( m_spline == NULL )
                return;

            // always the same at both ends

            m_spline->setBoundaryCondition( QwtSpline::AtBeginning, condition );
            m_spline->setBoundaryValue( QwtSpline::AtBeginning, value );

            m_spline->setBoundaryCondition( QwtSpline::AtEnd, condition );
            m_spline->setBoundaryValue( QwtSpline::AtEnd, value );
        }

        QwtSpline* m_spline;
    };

    class Curve : public QwtPlotCurve
    {
      public:
        Curve( const QString& title, const QColor& color )
            : QwtPlotCurve( title )
        {
            setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
            setCurveAttribute( QwtPlotCurve::Fitted, true );
            setRenderHint( QwtPlotItem::RenderAntialiased );

            setPen( color );
            setZ( 100 ); // on top of the marker
        }
    };

    class LineMarker : public QwtPlotMarker
    {
      public:
        LineMarker( const QString& title, const QColor& color )
            : QwtPlotMarker( title )
        {
            setLineStyle( QwtPlotMarker::VLine );
            setLinePen( QPen( color, 0, Qt::DotLine ) );

            QwtText text( "click on the axes" );
            text.setBackgroundBrush( Qt::white );
            text.setColor( Qt::darkRed );

            setLabel( text );
            setLabelOrientation( Qt::Vertical );
            setXValue( 5 );
        }
    };
}

Plot::Plot( bool parametric, QWidget* parent )
    : QwtPlot( parent )
    , m_boundaryCondition( QwtSplineC1::Clamped2 )
{
    setTitle( "Points can be dragged using the mouse" );

    setCanvasBackground( Qt::white );
#ifndef QT_NO_CURSOR
    canvas()->setCursor( Qt::PointingHandCursor );
#endif

    QwtLegend* legend = new QwtLegend;
    legend->setDefaultItemMode( QwtLegendData::Checkable );
    insertLegend( legend, QwtPlot::RightLegend );

    connect( legend, SIGNAL(checked(const QVariant&,bool,int)),
        SLOT(legendChecked(const QVariant&,bool)) );

    // Avoid jumping when label with 3 digits
    // appear/disappear when scrolling vertically

    QwtScaleDraw* sd = axisScaleDraw( QwtAxis::YLeft );
    sd->setMinimumExtent( sd->extent( axisWidget( QwtAxis::YLeft )->font() ) );

    // pointless marker
    m_marker = new LineMarker( "Marker", Qt::darkRed );
    m_marker->attach( this );

    // curves
    m_curve = new Curve( "Lines", QColor() );
    m_curve->setStyle( QwtPlotCurve::NoCurve );
    m_curve->setSymbol( new Symbol() );
    m_curve->setItemAttribute( QwtPlotItem::Legend, false );
    m_curve->setZ( 1000 );

    QPolygonF points;

    if ( parametric )
    {
        setAxisScale( QwtAxis::XBottom, 20.0, 80.0 );
        setAxisScale( QwtAxis::YLeft, -50.0, 100.0 );

        const QSizeF size( 40, 50 );
        const QPointF pos( 50, 70 );

        const double cos30 = 0.866025;

        const double dx = 0.5 * size.width() - cos30;
        const double dy = 0.25 * size.height();

        double x1 = pos.x() - dx;
        double y1 = pos.y() - 2 * dy;

        const double x2 = x1 + 1 * dx;
        const double x3 = x1 + 2 * dx;

        const double y2 = y1 + 1 * dy;
        const double y3 = y1 + 3 * dy;
        const double y4 = y1 + 4 * dy;

        points += QPointF( x2, y1 );
        points += QPointF( 0.5 * ( x2 + x3 ), y1 - 0.5 * ( y2 - y1 ) );
        points += QPointF( x3, y2 );
        points += QPointF( 0.5 * ( x2 + x3 ), 0.5 * ( y3 + y1 ) );
        points += QPointF( x3, y3 );
        points += QPointF( 0.5 * ( x2 + x3 ), y3 + 0.5 * ( y3 - y2 ) );
        points += QPointF( x2, y4 );
        points += QPointF( 0.5 * ( x1 + x2 ), y3 + 0.5 * ( y4 - y3 ) );
        points += QPointF( x1, y3 );
        points += QPointF( x1, y2 );
    }
    else
    {
        setAxisScale( QwtAxis::XBottom, 0.0, 100.0 );
        setAxisScale( QwtAxis::YLeft, -50.0, 100.0 );

        points << QPointF( 10, 30 ) << QPointF( 20, 90 ) << QPointF( 25, 60 )
               << QPointF( 35, 38 ) << QPointF( 42, 40 ) << QPointF( 55, 60 )
               << QPointF( 60, 50 ) << QPointF( 65, 80 ) << QPointF( 73, 30 )
               << QPointF( 82, 30 ) << QPointF( 87, 40 ) << QPointF( 95, 70 );
    }

    m_curve->setSamples( points );
    m_curve->attach( this );

    //

    Curve* curve;

    QVector< Curve* > curves;

    curve = new Curve( "Pleasing", "DarkGoldenRod" );
    curve->setCurveFitter( new SplineFitter( SplineFitter::PleasingSpline ) );
    curves += curve;

    curve = new Curve( "Cardinal", Qt::darkGreen);
    curve->setCurveFitter( new SplineFitter( SplineFitter::CardinalSpline ) );
    curves += curve;

    curve = new Curve( "PChip", Qt::darkYellow);
    curve->setCurveFitter( new SplineFitter( SplineFitter::PChipSpline ) );
    curves += curve;

    curve = new Curve( "Parabolic Blending", Qt::darkBlue);
    curve->setCurveFitter( new SplineFitter( SplineFitter::ParabolicBlendingSpline ) );
    curves += curve;

    curve = new Curve( "Akima", Qt::darkCyan);
    curve->setCurveFitter( new SplineFitter( SplineFitter::AkimaSpline ) );
    curves += curve;

    curve = new Curve( "Cubic", Qt::darkRed );
    curve->setCurveFitter( new SplineFitter( SplineFitter::CubicSpline ) );
    curves += curve;

    curve = new Curve( "Basis", QColor("DarkOliveGreen" ) );
    curve->setCurveFitter( new SplineFitter( SplineFitter::BasisSpline ) );
    curves += curve;

    for ( int i = 0; i < curves.size(); i++ )
    {
        curves[i]->attach( this );
        showCurve( curves[i], true );
    }

#if 0
    for ( int i = 0; i < curves.size(); i++ )
        showCurve( curves[i], false );

    showCurve( curves[0], true );
#endif

    setOverlaying( false );

    // ------------------------------------
    // The scale picker translates mouse clicks
    // on the bottom axis into clicked() signals
    // ------------------------------------

    ScalePicker* scalePicker = new ScalePicker( this );
    connect( scalePicker, SIGNAL(clicked(int,double)),
        this, SLOT(updateMarker(int,double)) );

    // ------------------------------------
    // The canvas picker handles all mouse and key
    // events on the plot canvas
    // ------------------------------------

    ( void ) new CanvasPicker( !parametric, this );

    // ------------------------------------
    // We add a wheel to the canvas
    // ------------------------------------

    m_wheel = new QwtWheel( canvas() );
    m_wheel->setOrientation( Qt::Vertical );
    m_wheel->setRange( -100, 100 );
    m_wheel->setValue( 0.0 );
    m_wheel->setMass( 0.2 );
    m_wheel->setTotalAngle( 4 * 360.0 );
    m_wheel->resize( 16, 60 );

    plotLayout()->setAlignCanvasToScale( QwtAxis::XTop, true );
    plotLayout()->setAlignCanvasToScale( QwtAxis::XBottom, true );
    plotLayout()->setAlignCanvasToScale( QwtAxis::YLeft, true );
    plotLayout()->setCanvasMargin( m_wheel->width() + 4, QwtAxis::YRight );

    connect( m_wheel, SIGNAL(valueChanged(double)),
        SLOT(scrollLeftAxis(double)) );

    // we need the resize events, to lay out the wheel
    canvas()->installEventFilter( this );

    m_wheel->setWhatsThis(
        "With the wheel you can move the visible area." );
    axisWidget( QwtAxis::XBottom )->setWhatsThis(
        "Selecting a value at the scale will insert a new curve." );
}

void Plot::scrollLeftAxis( double value )
{
    const double range = axisScaleDiv( QwtAxis::YLeft ).range();
    setAxisScale( QwtAxis::YLeft, value, value + range );
    replot();
}

bool Plot::eventFilter( QObject* object, QEvent* e )
{
    if ( e->type() == QEvent::Resize )
    {
        if ( object == canvas() )
        {
            const int margin = 2;

            const QRect cr = canvas()->contentsRect();
            m_wheel->move( cr.right() - margin - m_wheel->width(),
                cr.center().y() - ( m_wheel->height() ) / 2 );
        }
    }

    return QwtPlot::eventFilter( object, e );
}

void Plot::updateMarker( int axis, double value )
{
    if ( QwtAxis::isYAxis( axis ) )
    {
        m_marker->setLineStyle( QwtPlotMarker::HLine );
        m_marker->setLabelOrientation( Qt::Horizontal );
        m_marker->setYValue( value );
    }
    else
    {
        m_marker->setLineStyle( QwtPlotMarker::VLine );
        m_marker->setLabelOrientation( Qt::Vertical );
        m_marker->setXValue( value );
    }

    replot();
}


void Plot::legendChecked( const QVariant& itemInfo, bool on )
{
    QwtPlotItem* plotItem = infoToItem( itemInfo );
    if ( plotItem )
        showCurve( plotItem, on );
}

void Plot::showCurve( QwtPlotItem* item, bool on )
{
    item->setVisible( on );

    QwtLegend* lgd = qobject_cast< QwtLegend* >( legend() );

    QList< QWidget* > legendWidgets =
        lgd->legendWidgets( itemToInfo( item ) );

    if ( legendWidgets.size() == 1 )
    {
        QwtLegendLabel* legendLabel =
            qobject_cast< QwtLegendLabel* >( legendWidgets[0] );

        if ( legendLabel )
            legendLabel->setChecked( on );
    }

    replot();
}

void Plot::setClosed( bool on )
{
    QwtPlotItemList curves = itemList( QwtPlotItem::Rtti_PlotCurve );
    for ( int i = 0; i < curves.size(); i++ )
    {
        QwtPlotCurve* curve = dynamic_cast< QwtPlotCurve* >( curves[i] );

        SplineFitter* fitter = dynamic_cast< SplineFitter* >( curve->curveFitter() );
        if ( fitter )
            fitter->setClosing( on );
    }

    replot();
}

void Plot::setBoundaryCondition( const QString& condition )
{
    QwtPlotItemList curves = itemList( QwtPlotItem::Rtti_PlotCurve );
    for ( int i = 0; i < curves.size(); i++ )
    {
        QwtPlotCurve* curve = dynamic_cast< QwtPlotCurve* >( curves[i] );

        SplineFitter* fitter = dynamic_cast< SplineFitter* >( curve->curveFitter() );
        if ( fitter )
            fitter->setBoundaryCondition( condition );
    }

    replot();
}

void Plot::setParametric( const QString& parameterType )
{
    QwtPlotItemList curves = itemList( QwtPlotItem::Rtti_PlotCurve );
    for ( int i = 0; i < curves.size(); i++ )
    {
        QwtPlotCurve* curve = ( QwtPlotCurve*)curves[i];

        SplineFitter* fitter = dynamic_cast< SplineFitter* >( curve->curveFitter() );
        if ( fitter )
            fitter->setParametric( parameterType );
    }

    replot();
}

void Plot::setOverlaying( bool on )
{
    QPolygonF points;
    for ( size_t i = 0; i < m_curve->dataSize(); i++ )
        points += m_curve->sample( i );

    QwtPlotItemList curves = itemList( QwtPlotItem::Rtti_PlotCurve );

    for ( int i = 0; i < curves.size(); i++ )
    {
        QwtPlotCurve* curve = static_cast< QwtPlotCurve* >( curves[i] );
        if ( curve == m_curve )
            continue;

        QwtSymbol* symbol = NULL;

        if ( !on )
        {
            points.translate( 0.0, -10 );
            symbol = new Symbol();
        }

        curve->setSymbol( symbol );
        curve->setSamples( points );
    }

    m_curve->setVisible( on );

    replot();
}

#ifndef QT_NO_PRINTER

void Plot::printPlot()
{
    QPrinter printer( QPrinter::HighResolution );
#if QT_VERSION >= 0x050300
    printer.setPageOrientation( QPageLayout::Landscape );
#else
    printer.setOrientation( QPrinter::Landscape );
#endif
    printer.setOutputFileName( "spline.pdf" );

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

        renderer.renderTo( this, printer );
    }
}

#endif

#include "moc_Plot.cpp"
