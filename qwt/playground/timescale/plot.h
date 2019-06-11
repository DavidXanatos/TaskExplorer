#ifndef _PLOT_H_
#define _PLOT_H_

#include <qwt_plot.h>

class Settings;

class Plot: public QwtPlot
{
    Q_OBJECT 

public:
    Plot( QWidget *parent = NULL );

public Q_SLOTS:
    void applySettings( const Settings & );

private:
    void initAxis( int axis, const QString& title, Qt::TimeSpec );
    void applyAxisSettings( int axis, const Settings & );
};

#endif
