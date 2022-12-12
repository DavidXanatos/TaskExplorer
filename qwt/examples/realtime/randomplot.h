/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include "IncrementalPlot.h"
#include <QElapsedTimer>

class QTimer;

class RandomPlot : public IncrementalPlot
{
    Q_OBJECT

  public:
    RandomPlot( QWidget* parent = NULL );

    virtual QSize sizeHint() const QWT_OVERRIDE;

  Q_SIGNALS:
    void running( bool );
    void elapsed( int ms );

  public Q_SLOTS:
    void clear();
    void stop();
    void append( int timeout, int count );

  private Q_SLOTS:
    void appendPoint();

  private:
    void initCurve();

    QTimer* m_timer;
    int m_timerCount;

    QElapsedTimer m_timeStamp;
};
