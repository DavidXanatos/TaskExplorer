/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#pragma once

#include <QWidget>

class QLabel;
class QwtThermo;
class QwtWheel;

class WheelBox : public QWidget
{
    Q_OBJECT
  public:
    WheelBox( Qt::Orientation,
        int type, QWidget* parent = NULL );

  private Q_SLOTS:
    void setNum( double v );

  private:
    QWidget* createBox( Qt::Orientation, int type );

  private:
    QwtWheel* m_wheel;
    QwtThermo* m_thermo;
    QLabel* m_label;
};
