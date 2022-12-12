/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "DialTab.h"
#include "DialBox.h"

#include <QLayout>

DialTab::DialTab( QWidget* parent )
    : QWidget( parent )
{
    QGridLayout* layout = new QGridLayout( this );

    const int numRows = 3;
    for ( int i = 0; i < 2 * numRows; i++ )
    {
        DialBox* dialBox = new DialBox( this, i );
        layout->addWidget( dialBox, i / numRows, i % numRows );
    }
}

