#ifndef _CURVE_TRACKER_H_
#define _CURVE_TRACKER_H_

#include <qwt_plot_picker.h>

class QwtPlotCurve;

class CurveTracker: public QwtPlotPicker
{
public:
    CurveTracker( QWidget * );

protected:
    virtual QwtText trackerTextF( const QPointF & ) const;
    virtual QRect trackerRect( const QFont & ) const;

private:
    QString curveInfoAt( const QwtPlotCurve *, const QPointF & ) const;
    QLineF curveLineAt( const QwtPlotCurve *, double x ) const;
};

#endif
