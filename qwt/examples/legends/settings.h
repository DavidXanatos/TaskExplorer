/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QString>

class Settings
{
  public:
    Settings()
    {
        legend.isEnabled = false;
        legend.position = 0;

        legendItem.isEnabled = false;
        legendItem.numColumns = 0;
        legendItem.alignment = 0;
        legendItem.backgroundMode = 0;
        legendItem.size = 12;

        curve.numCurves = 0;
        curve.title = "Curve";
    }

    struct
    {
        bool isEnabled;
        int position;
    } legend;

    struct
    {
        bool isEnabled;
        int numColumns;
        int alignment;
        int backgroundMode;
        int size;

    } legendItem;

    struct
    {
        int numCurves;
        QString title;
    } curve;
};
