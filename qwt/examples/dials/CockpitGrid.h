/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QFrame>

class QwtDial;

class CockpitGrid : public QFrame
{
  public:
    CockpitGrid( QWidget* parent = NULL );

  private:
    QwtDial* createDial( int pos );
};
