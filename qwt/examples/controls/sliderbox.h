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
class QwtSlider;

class SliderBox : public QWidget
{
    Q_OBJECT
  public:
    SliderBox( int sliderType, QWidget* parent = NULL );

  private Q_SLOTS:
    void setNum( double v );

  private:
    QwtSlider* createSlider( int sliderType ) const;

    QwtSlider* m_slider;
    QLabel* m_label;
};
