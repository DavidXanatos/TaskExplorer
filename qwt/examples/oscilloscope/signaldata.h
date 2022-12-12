/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QRect>

class SignalData
{
  public:
    static SignalData& instance();

    void append( const QPointF& pos );
    void clearStaleValues( double min );

    int size() const;
    QPointF value( int index ) const;

    QRectF boundingRect() const;

    void lock();
    void unlock();

  private:
    SignalData();
    ~SignalData();

    Q_DISABLE_COPY( SignalData )

    class PrivateData;
    PrivateData* m_data;
};
