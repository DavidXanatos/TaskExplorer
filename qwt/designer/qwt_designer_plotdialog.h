/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#ifndef QWT_DESIGNER_PLOTDIALOG_H
#define QWT_DESIGNER_PLOTDIALOG_H

#include <QDialog>

namespace QwtDesignerPlugin
{

    class PlotDialog: public QDialog
    {
        Q_OBJECT

    public:
        PlotDialog( const QString &properties, QWidget *parent = NULL );

    Q_SIGNALS:
        void edited( const QString& );
    };

}

#endif
