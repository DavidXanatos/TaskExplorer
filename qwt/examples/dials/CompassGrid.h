/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QFrame>

class QwtCompass;

class CompassGrid : public QFrame
{
  public:
    CompassGrid( QWidget* parent = NULL );

  private:
    QwtCompass* createCompass( int pos );
};
