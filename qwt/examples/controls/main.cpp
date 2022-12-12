/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "SliderTab.h"
#include "WheelTab.h"
#include "KnobTab.h"
#include "DialTab.h"

#include <QApplication>
#include <QTabWidget>

namespace
{
    class TabWidget : public QTabWidget
    {
      public:
        TabWidget()
        {
            SliderTab* sliderTab = new SliderTab();
            sliderTab->setAutoFillBackground( true );
            sliderTab->setPalette( QColor( "DimGray" ) );

            WheelTab* wheelTab = new WheelTab();
            wheelTab->setAutoFillBackground( true );
            wheelTab->setPalette( QColor( "Silver" ) );

            KnobTab* knobTab = new KnobTab();
            knobTab->setAutoFillBackground( true );
            knobTab->setPalette( Qt::darkGray );

            DialTab* dialTab = new DialTab();
            dialTab->setAutoFillBackground( true );
            dialTab->setPalette( Qt::darkGray );

            addTab( sliderTab, "Slider" );
            addTab( wheelTab, "Wheel/Thermo" );
            addTab( knobTab, "Knob" );
            addTab( dialTab, "Dial" );
        }
    };
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    TabWidget tabWidget;

    tabWidget.resize( 800, 600 );
    tabWidget.show();

    return app.exec();
}

