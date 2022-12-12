/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "SliderTab.h"
#include "SliderBox.h"

#include <QLayout>

SliderTab::SliderTab( QWidget* parent )
    : QWidget( parent )
{
    int i;

    QBoxLayout* hLayout = createLayout( Qt::Vertical );
    for ( i = 0; i < 4; i++ )
        hLayout->addWidget( new SliderBox( i ) );
    hLayout->addStretch();

    QBoxLayout* vLayout = createLayout( Qt::Horizontal );
    for ( ; i < 7; i++ )
        vLayout->addWidget( new SliderBox( i ) );

    QBoxLayout* mainLayout = createLayout( Qt::Horizontal, this );
    mainLayout->addLayout( vLayout );
    mainLayout->addLayout( hLayout, 10 );
}

QBoxLayout* SliderTab::createLayout(
    Qt::Orientation orientation, QWidget* widget )
{
    QBoxLayout* layout =
        new QBoxLayout( QBoxLayout::LeftToRight, widget );

    if ( orientation == Qt::Vertical )
        layout->setDirection( QBoxLayout::TopToBottom );

    layout->setSpacing( 20 );
    layout->setContentsMargins( QMargins() );

    return layout;
}
