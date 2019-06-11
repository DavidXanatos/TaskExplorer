#ifndef _PLOT_MATRIX_H_
#define _PLOT_MATRIX_H_

#include <qframe.h>
#include <qwt_plot.h>

class PlotMatrix: public QFrame
{
    Q_OBJECT

public:
    PlotMatrix( int rows, int columns, QWidget * parent = NULL );
    virtual ~PlotMatrix();

    int numRows() const;
    int numColumns() const;

    QwtPlot* plotAt( int row, int column );
    const QwtPlot* plotAt( int row, int column ) const;

    void enableAxis( int axisId, bool tf = true );
    bool axisEnabled( int axisId ) const;

    void setAxisScale( int axisId, int rowOrColumn,
        double min, double max, double step = 0 );

protected:
    void updateLayout();

private Q_SLOTS:
    void scaleDivChanged();

private:
    void alignAxes( int rowOrColumn, int axis );
    void alignScaleBorder( int rowOrColumn, int axis );

    class PrivateData;
    PrivateData *d_data;
};

#endif
