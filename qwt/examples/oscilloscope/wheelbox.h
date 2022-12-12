/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#pragma once

#include <QWidget>

class QwtWheel;
class QLabel;
class QLCDNumber;

class WheelBox : public QWidget
{
    Q_OBJECT
    Q_PROPERTY( QColor theme READ theme WRITE setTheme )

  public:
    WheelBox( const QString& title,
        double min, double max, double stepSize,
        QWidget* parent = NULL );

    void setTheme( const QColor& );
    QColor theme() const;

    void setUnit( const QString& );
    QString unit() const;

    void setValue( double value );
    double value() const;

  Q_SIGNALS:
    double valueChanged( double );

  private:
    QLCDNumber* m_number;
    QwtWheel* m_wheel;
    QLabel* m_label;

    QString m_unit;
};
