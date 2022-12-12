/*****************************************************************************
 * Qwt Polar Examples - Copyright (C) 2008   Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include "Plot.h"
#include <QFrame>

class QCheckBox;

class SettingsEditor : public QFrame
{
    Q_OBJECT

  public:
    SettingsEditor( QWidget* parent = NULL );

    void showSettings( const PlotSettings& );
    PlotSettings settings() const;

  Q_SIGNALS:
    void edited( const PlotSettings& );

  private Q_SLOTS:
    void edited();

  private:
    void updateEditor();
    QString label( int flag ) const;

    QCheckBox* m_checkBox[PlotSettings::NumFlags];
};
