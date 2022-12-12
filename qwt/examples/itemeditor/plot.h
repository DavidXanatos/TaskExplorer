/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include "ShapeFactory.h"
#include <QwtPlot>

class QColor;
class QSizeF;
class QPointF;
class Editor;

class Plot : public QwtPlot
{
    Q_OBJECT

  public:
    Plot( QWidget* parent = NULL );

  public Q_SLOTS:
    void exportPlot();
    void setMode( int );

  private:
    void populate();

    void addShape( const QString& title,
        ShapeFactory::Shape, const QColor&,
        const QPointF&, const QSizeF& );

    Editor* m_editor;
};
