#ifndef _PLOT_H_
#define _PLOT_H_

#include <qwt_plot.h>

class QPolygonF;

class Plot: public QwtPlot
{
    Q_OBJECT

public:
    Plot( QWidget * = NULL );

private:
    void insertCurve( const QString &title, 
        const QColor &, const QPolygonF & );
};

#endif

