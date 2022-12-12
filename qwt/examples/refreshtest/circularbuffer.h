/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtSeriesData>
#include <QVector>

class CircularBuffer : public QwtSeriesData< QPointF >
{
  public:
    CircularBuffer( double interval = 10.0, size_t numPoints = 1000 );
    void fill( double interval, size_t numPoints );

    void setReferenceTime( double );
    double referenceTime() const;

    virtual size_t size() const QWT_OVERRIDE;
    virtual QPointF sample( size_t index ) const QWT_OVERRIDE;

    virtual QRectF boundingRect() const QWT_OVERRIDE;

    void setFunction( double ( * y )( double ) );

  private:
    double ( * m_y )( double );

    double m_referenceTime;
    double m_interval;
    QVector< double > m_values;

    double m_step;
    int m_startIndex;
    double m_offset;
};
