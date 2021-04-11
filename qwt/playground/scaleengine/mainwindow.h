#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <qmainwindow.h>

class Plot;
class TransformPlot;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow( QWidget *parent = 0 );

private:
    Plot *d_plot;
    TransformPlot *d_transformPlot;
};

#endif
