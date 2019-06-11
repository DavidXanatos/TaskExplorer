#include <qwidget.h>

class QByteArray;
class QSvgRenderer;
class QwtGraphic;

class Canvas: public QWidget
{
public:
    enum Mode
    {
        Svg,
        VectorGraphic
    };

    Canvas( Mode, QWidget *parent = NULL );
    virtual ~Canvas();

    void setSvg( const QByteArray & );

protected:
    virtual void paintEvent( QPaintEvent * );

private:
    void render( QPainter *, const QRect & ) const;

    const Mode d_mode;
    union
    {
        QSvgRenderer *d_renderer;
        QwtGraphic *d_graphic;
    };
};
