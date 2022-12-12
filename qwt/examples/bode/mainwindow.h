/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#pragma once

#include <QMainWindow>

class QwtPlotZoomer;
class QwtPlotPicker;
class QwtPlotPanner;
class Plot;
class QPolygon;

class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    MainWindow( QWidget* parent = 0 );

  private Q_SLOTS:
    void moved( const QPoint& );
    void selected( const QPolygon& );

#ifndef QT_NO_PRINTER
    void print();
#endif

    void exportDocument();
    void enableZoomMode( bool );

  private:
    void showInfo( QString text = QString() );

    Plot* m_plot;

    QwtPlotZoomer* m_zoomer[2];
    QwtPlotPicker* m_picker;
    QwtPlotPanner* m_panner;
};
