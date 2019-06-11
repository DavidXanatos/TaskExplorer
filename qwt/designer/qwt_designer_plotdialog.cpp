/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include <QLineEdit>
#include <QTabWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include "qwt_designer_plotdialog.h"

using namespace QwtDesignerPlugin;

PlotDialog::PlotDialog( const QString &properties, QWidget *parent ):
    QDialog( parent )
{
    setWindowTitle( "Plot Properties" );

    QLineEdit *lineEdit = new QLineEdit( properties );
    connect( lineEdit, SIGNAL( textChanged( const QString & ) ),
        SIGNAL( edited( const QString & ) ) );

    QTabWidget *tabWidget = new QTabWidget( this );
    tabWidget->addTab( lineEdit, "General" );

    QPushButton *closeButton = new QPushButton( "Close" );
    connect( closeButton, SIGNAL( clicked() ), this, SLOT( accept() ) );

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch( 1 );
    buttonLayout->addWidget( closeButton );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget( tabWidget );
    mainLayout->addLayout( buttonLayout );
    setLayout( mainLayout );
}

