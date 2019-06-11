#include <qapplication.h>
#include <qpainter.h>
#include <qbuffer.h>
#ifdef QT_SVG_LIB
#include <qsvggenerator.h>
#endif
#include <qwt_plot.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_symbol.h>
#include <qwt_graphic.h>
#include <qwt_legend.h>

class MySymbol: public QwtSymbol
{
public:
    MySymbol( QwtSymbol::Style style, const QBrush &brush )
    {
        QPen pen( Qt::black, 0 );
        pen.setJoinStyle( Qt::MiterJoin );
        pen.setCosmetic( true );

        QPainterPath path = createArrow( QSize( 16, 24 ) );

        const QSizeF pathSize = path.boundingRect().size();

        setSize( 0.8 * pathSize.toSize() );

        setPinPoint( QPointF( 0.0, 0.0 ) );

        switch( style )
        {
            case QwtSymbol::Pixmap:
            {
                const QSize sz = size();

                const double ratio = qMin( sz.width() / pathSize.width(),
                    sz.height() / pathSize.height() );

                QTransform transform;
                transform.scale( ratio, ratio );

                path = transform.map( path );

                if ( isPinPointEnabled() )
                {
                    QPointF pos = transform.map( pinPoint() );
                    setPinPoint( pos );
                }

                const QRectF br = path.boundingRect();

                int m = 2 + qCeil( pen.widthF() );

                QPixmap pm( sz + QSize( 2 * m, 2 * m ) );
                pm.fill( Qt::transparent );

                QPainter painter( &pm );
                painter.setRenderHint( QPainter::Antialiasing, true );
                
                painter.setPen( pen ); 
                painter.setBrush( brush );

                painter.translate( m, m );
                painter.translate( -br.left(), br.top() );
                painter.drawPath( path );
                
                setPixmap( pm );
                setSize( pm.size() );
                if ( isPinPointEnabled() )
                    setPinPoint( pinPoint() + QPointF( m, m ) );

                break;
            }
            case QwtSymbol::Graphic:
            {
                QwtGraphic graphic;
                graphic.setRenderHint( QwtGraphic::RenderPensUnscaled );
        
                QPainter painter( &graphic );
                painter.setRenderHint( QPainter::Antialiasing, true );
                painter.setPen( pen ); 
                painter.setBrush( brush );
        
                painter.drawPath( path );
                painter.end();
        
                setGraphic( graphic );
                break;
            }
            case QwtSymbol::SvgDocument:
            {
#ifndef QWT_NO_SVG
                QBuffer buf;

                QSvgGenerator generator;
                generator.setOutputDevice( &buf );

                QPainter painter( &generator );
                painter.setRenderHint( QPainter::Antialiasing, true );
                painter.setPen( pen );
                painter.setBrush( brush );

                painter.drawPath( path );
                painter.end();

                setSvgDocument( buf.data() );
#endif
                break;
            }
            case QwtSymbol::Path:
            default:
            {
                setPen( pen );
                setBrush( brush );
                setPath( path );
            }
        }

    }

private:
    QPainterPath createArrow( const QSizeF &size ) const
    {
        const double w = size.width();
        const double h = size.height();
        const double y0 = 0.6 * h;

        QPainterPath path; 
        path.moveTo( 0, h );
        path.lineTo( 0, y0 );
        path.lineTo( -0.5 * w, y0 );
        path.lineTo( 0, 0 );
        path.lineTo( 0.5 * w, y0 );
        path.lineTo( 0, y0 );
        
        QTransform transform;
        transform.rotate( -30.0 );
        path = transform.map( path );

        return path;
    }
};

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    QwtPlot plot;
    plot.setTitle( "Plot Demo" );
    plot.setCanvasBackground( Qt::white );

    plot.setAxisScale( QwtPlot::xBottom, -1.0, 6.0 );

    QwtLegend *legend = new QwtLegend();
    legend->setDefaultItemMode( QwtLegendData::Checkable );
    plot.insertLegend( legend );

    for ( int i = 0; i < 4; i++ )
    {
        QwtPlotCurve *curve = new QwtPlotCurve();
        curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        curve->setPen( Qt::blue );

        QBrush brush;
        QwtSymbol::Style style = QwtSymbol::NoSymbol;
        QString title;
        if ( i == 0 )
        {
            brush = Qt::magenta;
            style = QwtSymbol::Path;
            title = "Path";
        }
        else if ( i == 2 )
        {
            brush = Qt::red;
            style = QwtSymbol::Graphic;
            title = "Graphic";
        }
        else if ( i == 1 )
        {
            brush = Qt::yellow;
            style = QwtSymbol::SvgDocument;
            title = "Svg";
        }
        else if ( i == 3 )
        {
            brush = Qt::cyan;
            style = QwtSymbol::Pixmap;
            title = "Pixmap";
        }

        MySymbol *symbol = new MySymbol( style, brush );

        curve->setSymbol( symbol );
        curve->setTitle( title );
        curve->setLegendAttribute( QwtPlotCurve::LegendShowSymbol, true );
        curve->setLegendIconSize( QSize( 15, 18 ) );

        QPolygonF points;
        points << QPointF( 0.0, 4.4 ) << QPointF( 1.0, 3.0 )
            << QPointF( 2.0, 4.5 ) << QPointF( 3.0, 6.8 )
            << QPointF( 4.0, 7.9 ) << QPointF( 5.0, 7.1 );

        points.translate( 0.0, i * 2.0 );

        curve->setSamples( points );
        curve->attach( &plot );
    }

    plot.resize( 600, 400 );
    plot.show();

    return a.exec();
}
