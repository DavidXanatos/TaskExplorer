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

class QBoxLayout;

class SliderTab : public QWidget
{
  public:
    SliderTab( QWidget* parent = NULL );

  private:
    QBoxLayout* createLayout( Qt::Orientation,
        QWidget* widget = NULL );
};
