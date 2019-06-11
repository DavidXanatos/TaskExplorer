#ifndef _TRANSFORM_PLOT_H_
#define _TRANSFORM_PLOT_H_

#include <qwt_plot.h>

class TransformPlot: public QwtPlot
{
    Q_OBJECT

public:
    TransformPlot( QWidget *parent = NULL );
    void insertTransformation( const QString &, 
        const QColor &, QwtTransform * );

    void setLegendChecked( QwtPlotItem * );

Q_SIGNALS:
    void selected( QwtTransform * );

private Q_SLOTS:
    void legendChecked( const QVariant &, bool on );

private:
};

#endif

