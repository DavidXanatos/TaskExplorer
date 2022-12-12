/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QDateTime>

class Settings
{
  public:
    Settings()
        : maxMajorSteps( 10 )
        , maxMinorSteps( 5 )
        , maxWeeks( -1 )
    {
    };

    QDateTime startDateTime;
    QDateTime endDateTime;

    int maxMajorSteps;
    int maxMinorSteps;

    int maxWeeks;
};
