/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include <QwtScaleMap>
#include <QwtPlotCurve>
#include <QwtSymbol>
#include <QwtMath>

#include <QPainter>
#include <QApplication>
#include <QFrame>

const int Size = 27;
const int CurvCnt = 6;

namespace
{
    class CurveBox : public QFrame
    {
      public:
        CurveBox();

      protected:
        virtual void paintEvent( QPaintEvent* ) QWT_OVERRIDE;

      private:
        void drawCurves( QPainter* );
        void populate();
        void shiftDown( QRect& rect, int offset ) const;

        QwtPlotCurve m_curves[CurvCnt];

        QwtScaleMap xMap;
        QwtScaleMap yMap;

        double xval[Size];
        double yval[Size];
    };
}

CurveBox::CurveBox()
{
    xMap.setScaleInterval( -0.5, 10.5 );
    yMap.setScaleInterval( -1.1, 1.1 );

    setFrameStyle( QFrame::Box | QFrame::Raised );
    setLineWidth( 2 );
    setMidLineWidth( 3 );

    for( int i = 0; i < Size; i++ )
    {
        xval[i] = double( i ) * 10.0 / double( Size - 1 );
        yval[i] = std::sin( xval[i] ) * std::cos( 2.0 * xval[i] );
    }

    populate();
}

void CurveBox::populate()
{
    int i = 0;

    m_curves[i].setSymbol( new QwtSymbol( QwtSymbol::Cross, Qt::NoBrush,
        QPen( Qt::black ), QSize( 5, 5 ) ) );
    m_curves[i].setPen( Qt::darkGreen );
    m_curves[i].setStyle( QwtPlotCurve::Lines );
    m_curves[i].setCurveAttribute( QwtPlotCurve::Fitted );
    i++;

    m_curves[i].setSymbol( new QwtSymbol( QwtSymbol::Ellipse, Qt::yellow,
        QPen( Qt::blue ), QSize( 5, 5 ) ) );
    m_curves[i].setPen( Qt::red );
    m_curves[i].setStyle( QwtPlotCurve::Sticks );
    i++;

    m_curves[i].setPen( Qt::darkBlue );
    m_curves[i].setStyle( QwtPlotCurve::Lines );
    i++;

    m_curves[i].setPen( Qt::darkBlue );
    m_curves[i].setStyle( QwtPlotCurve::Lines );
    m_curves[i].setRenderHint( QwtPlotItem::RenderAntialiased );
    i++;

    m_curves[i].setPen( Qt::darkCyan );
    m_curves[i].setStyle( QwtPlotCurve::Steps );
    i++;

    m_curves[i].setSymbol( new QwtSymbol( QwtSymbol::XCross, Qt::NoBrush,
        QPen( Qt::darkMagenta ), QSize( 5, 5 ) ) );
    m_curves[i].setStyle( QwtPlotCurve::NoCurve );
    i++;

    for( i = 0; i < CurvCnt; i++ )
        m_curves[i].setRawSamples( xval, yval, Size );
}

void CurveBox::shiftDown( QRect& rect, int offset ) const
{
    rect.translate( 0, offset );
}

void CurveBox::paintEvent( QPaintEvent* event )
{
    QFrame::paintEvent( event );

    QPainter painter( this );
    painter.setClipRect( contentsRect() );

    drawCurves( &painter );
}

void CurveBox::drawCurves( QPainter* painter )
{
    int deltay, i;

    QRect r = contentsRect();

    deltay = r.height() / CurvCnt - 1;

    r.setHeight( deltay );

    //  draw curves
    for ( i = 0; i < CurvCnt; i++ )
    {
        xMap.setPaintInterval( r.left(), r.right() );
        yMap.setPaintInterval( r.top(), r.bottom() );

        painter->setRenderHint( QPainter::Antialiasing,
            m_curves[i].testRenderHint( QwtPlotItem::RenderAntialiased ) );
        m_curves[i].draw( painter, xMap, yMap, r );

        shiftDown( r, deltay );
    }

    // draw titles
    r = contentsRect();
    painter->setFont( QFont( "Helvetica", 8 ) );

    const int alignment = Qt::AlignTop | Qt::AlignHCenter;

    painter->setPen( Qt::black );

    painter->drawText( 0, r.top(), r.width(), painter->fontMetrics().height(),
        alignment, "Style: Line/Fitted, Symbol: Cross" );
    shiftDown( r, deltay );

    painter->drawText( 0, r.top(), r.width(), painter->fontMetrics().height(),
        alignment, "Style: Sticks, Symbol: Ellipse" );
    shiftDown( r, deltay );

    painter->drawText( 0, r.top(), r.width(), painter->fontMetrics().height(),
        alignment, "Style: Lines, Symbol: None" );
    shiftDown( r, deltay );

    painter->drawText( 0, r.top(), r.width(), painter->fontMetrics().height(),
        alignment, "Style: Lines, Symbol: None, Antialiased" );
    shiftDown( r, deltay );

    painter->drawText( 0, r.top(), r.width(), painter->fontMetrics().height(),
        alignment, "Style: Steps, Symbol: None" );
    shiftDown( r, deltay );

    painter->drawText( 0, r.top(), r.width(), painter->fontMetrics().height(),
        alignment, "Style: NoCurve, Symbol: XCross" );
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    CurveBox curveBox;

    curveBox.resize( 300, 600 );
    curveBox.show();

    return app.exec();
}
