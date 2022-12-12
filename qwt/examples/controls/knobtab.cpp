/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "KnobTab.h"
#include "KnobBox.h"

#include <QLayout>

KnobTab::KnobTab( QWidget* parent )
    : QWidget( parent )
{
    QGridLayout* layout = new QGridLayout( this );

    const int numRows = 3;
    for ( int i = 0; i < 2 * numRows; i++ )
    {
        KnobBox* knobBox = new KnobBox( this, i );
        layout->addWidget( knobBox, i / numRows, i % numRows );
    }
}

