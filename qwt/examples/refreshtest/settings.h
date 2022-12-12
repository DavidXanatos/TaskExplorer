/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QPen>
#include <QBrush>

class Settings
{
  public:
    enum FunctionType
    {
        NoFunction = -1,

        Wave,
        Noise
    };

    enum UpdateType
    {
        RepaintCanvas,
        Replot
    };

    Settings()
    {
        grid.pen = Qt::NoPen;
        grid.pen.setCosmetic( true );

        curve.brush = Qt::NoBrush;
        curve.numPoints = 1000;
        curve.functionType = Wave;
        curve.paintAttributes = 0;
        curve.renderHint = 0;
        curve.lineSplitting = true;

        canvas.useBackingStore = false;
        canvas.paintOnScreen = false;
        canvas.immediatePaint = true;
#ifndef QWT_NO_OPENGL
        canvas.openGL = false;
#endif

        updateType = RepaintCanvas;
        updateInterval = 20;
    }

    struct gridSettings
    {
        QPen pen;
    } grid;

    struct curveSettings
    {
        QPen pen;
        QBrush brush;
        uint numPoints;
        FunctionType functionType;
        int paintAttributes;
        int renderHint;
        bool lineSplitting;
    } curve;

    struct canvasSettings
    {
        bool useBackingStore;
        bool paintOnScreen;
        bool immediatePaint;

#ifndef QWT_NO_OPENGL
        bool openGL;
#endif
    } canvas;

    UpdateType updateType;
    int updateInterval;
};
