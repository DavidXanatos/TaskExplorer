/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

class Temperature
{
  public:
    Temperature():
        minValue( 0.0 ),
        maxValue( 0.0 ),
        averageValue( 0.0 )
    {
    }

    Temperature( double min, double max, double average ):
        minValue( min ),
        maxValue( max ),
        averageValue( average )
    {
    }

    double minValue;
    double maxValue;
    double averageValue;
};

extern Temperature friedberg2007[];
