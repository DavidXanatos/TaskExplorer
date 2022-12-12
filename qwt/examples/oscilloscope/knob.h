/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QwtGlobal>
#include <QWidget>

class QwtKnob;
class QLabel;

class Knob : public QWidget
{
    Q_OBJECT

    Q_PROPERTY( QColor theme READ theme WRITE setTheme )

  public:
    Knob( const QString& title,
        double min, double max, QWidget* parent = NULL );

    virtual QSize sizeHint() const QWT_OVERRIDE;

    void setValue( double value );
    double value() const;

    void setTheme( const QColor& );
    QColor theme() const;

  Q_SIGNALS:
    double valueChanged( double );

  protected:
    virtual void resizeEvent( QResizeEvent* ) QWT_OVERRIDE;

  private:
    QwtKnob* m_knob;
    QLabel* m_label;
};
