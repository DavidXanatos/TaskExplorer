/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include "Settings.h"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLineEdit;

class Panel : public QWidget
{
    Q_OBJECT

  public:
    Panel( QWidget* parent = NULL );

    void setSettings( const Settings&);
    Settings settings() const;

  Q_SIGNALS:
    void edited();

  private:
    struct
    {
        QCheckBox* checkBox;
        QComboBox* positionBox;

    } m_legend;

    struct
    {
        QCheckBox* checkBox;
        QSpinBox* numColumnsBox;
        QComboBox* hAlignmentBox;
        QComboBox* vAlignmentBox;
        QComboBox* backgroundBox;
        QSpinBox* sizeBox;

    } m_legendItem;

    struct
    {
        QSpinBox* numCurves;
        QLineEdit* title;

    } m_curve;
};
