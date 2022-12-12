/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "WheelTab.h"
#include "WheelBox.h"

#include <QLayout>

WheelTab::WheelTab( QWidget* parent )
    : QWidget( parent )
{
    const int numBoxes = 4;

    QGridLayout* layout1 = new QGridLayout();
    for ( int i = 0; i < numBoxes; i++ )
    {
        WheelBox* box = new WheelBox( Qt::Vertical, i );
        layout1->addWidget( box, i / 2, i % 2 );
    }

    QGridLayout* layout2 = new QGridLayout();
    for ( int i = 0; i < numBoxes; i++ )
    {
        WheelBox* box = new WheelBox( Qt::Horizontal, i + numBoxes );
        layout2->addWidget( box, i / 2, i % 2 );
    }

    QHBoxLayout* layout = new QHBoxLayout( this );
    layout->addLayout( layout1, 2 );
    layout->addLayout( layout2, 5 );
}

