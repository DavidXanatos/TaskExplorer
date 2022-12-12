/*****************************************************************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#pragma once

#include <QwtGlobal>
#include <QFrame>

class Knob;
class Thermo;

class AmplifierBox : public QFrame
{
    Q_OBJECT

  public:
    AmplifierBox( QWidget* = NULL );

  public Q_SLOTS:
    void setMaster( double v );

  protected:
    virtual void timerEvent( QTimerEvent* ) QWT_OVERRIDE;

  private:
    Knob* m_knobVolume;
    Knob* m_knobBalance;
    Knob* m_knobTreble;
    Knob* m_knobBass;

    Thermo* m_gaugeLeft;
    Thermo* m_gaugeRight;

    double m_master;
};
