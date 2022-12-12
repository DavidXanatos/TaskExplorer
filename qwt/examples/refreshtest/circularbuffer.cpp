/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "CircularBuffer.h"
#include <QwtMath>

CircularBuffer::CircularBuffer( double interval, size_t numPoints )
    : m_y( NULL )
    , m_referenceTime( 0.0 )
    , m_startIndex( 0 )
    , m_offset( 0.0 )
{
    fill( interval, numPoints );
}

void CircularBuffer::fill( double interval, size_t numPoints )
{
    if ( interval <= 0.0 || numPoints < 2 )
        return;

    m_values.resize( numPoints );
    m_values.fill( 0.0 );

    if ( m_y )
    {
        m_step = interval / ( numPoints - 2 );
        for ( size_t i = 0; i < numPoints; i++ )
            m_values[i] = m_y( i * m_step );
    }

    m_interval = interval;
}

void CircularBuffer::setFunction( double ( * y )( double ) )
{
    m_y = y;
}

void CircularBuffer::setReferenceTime( double timeStamp )
{
    m_referenceTime = timeStamp;

    const double startTime = std::fmod( m_referenceTime, m_values.size() * m_step );

    m_startIndex = int( startTime / m_step ); // floor
    m_offset = std::fmod( startTime, m_step );
}

double CircularBuffer::referenceTime() const
{
    return m_referenceTime;
}

size_t CircularBuffer::size() const
{
    return m_values.size();
}

QPointF CircularBuffer::sample( size_t i ) const
{
    const int size = m_values.size();

    int index = m_startIndex + i;
    if ( index >= size )
        index -= size;

    const double x = i * m_step - m_offset - m_interval;
    const double y = m_values.data()[index];

    return QPointF( x, y );
}

QRectF CircularBuffer::boundingRect() const
{
    return QRectF( -1.0, -m_interval, 2.0, m_interval );
}
