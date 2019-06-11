#ifndef _PLOT_H_
#define _PLOT_H_

#include <qwt_plot.h>

class QwtTransform;

class Plot: public QwtPlot
{
    Q_OBJECT

public:
    Plot( QWidget *parent = NULL );

public Q_SLOTS:
    void setTransformation( QwtTransform * );

private:
    void populate();
};

#endif

