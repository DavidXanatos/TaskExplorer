/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QScrollBar>

class ScrollBar : public QScrollBar
{
    Q_OBJECT

  public:
    ScrollBar( QWidget* parent = NULL );
    ScrollBar( Qt::Orientation, QWidget* parent = NULL );
    ScrollBar( double minBase, double maxBase,
        Qt::Orientation o, QWidget* parent = NULL );

    void setInverted( bool );
    bool isInverted() const;

    double minBaseValue() const;
    double maxBaseValue() const;

    double minSliderValue() const;
    double maxSliderValue() const;

    int extent() const;

  Q_SIGNALS:
    void sliderMoved( Qt::Orientation, double, double );
    void valueChanged( Qt::Orientation, double, double );

  public Q_SLOTS:
    virtual void setBase( double min, double max );
    virtual void moveSlider( double min, double max );

  protected:
    void sliderRange( int value, double& min, double& max ) const;
    int mapToTick( double ) const;
    double mapFromTick( int ) const;

  private Q_SLOTS:
    void catchValueChanged( int value );
    void catchSliderMoved( int value );

  private:
    void init();

    bool m_inverted;
    double m_minBase;
    double m_maxBase;
    int m_baseTicks;
};
