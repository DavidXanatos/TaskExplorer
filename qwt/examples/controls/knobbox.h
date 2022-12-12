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
class QwtKnob;

class KnobBox : public QWidget
{
    Q_OBJECT

  public:
    KnobBox( QWidget* parent, int knobType );

  private Q_SLOTS:
    void setNum( double v );

  private:
    QwtKnob* createKnob( int knobType ) const;

    QwtKnob* m_knob;
    QLabel* m_label;
};
