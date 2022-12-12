/*****************************************************************************
* Qwt Examples - Copyright (C) 2002 Uwe Rathmann
* This file may be used under the terms of the 3-clause BSD License
*****************************************************************************/

#include <QwtSplineCubic>
#include <QwtSplineLocal>
#include <QwtSplineParametrization>
#include <QwtSplinePolynomial>
#include <QwtMath>

#include <QPolygon>
#include <QPainterPath>
#include <QDebug>

#define DEBUG_ERRORS 1

static inline bool fuzzyCompare( double a, double b )
{
    return ( qFuzzyIsNull(a) && qFuzzyIsNull(b) ) || qFuzzyCompare(a, b);
}

static QwtSplinePolynomial polynomialAt( int index,
    const QPolygonF& points, const QVector< double >& m )
{
    return QwtSplinePolynomial::fromSlopes(
        points[index], m[index], points[index + 1], m[index + 1] );
}

class CubicSpline : public QwtSplineCubic
{
  public:
    CubicSpline( const QString& name )
        : d_name( name )
    {
    }

    void setBoundaryConditions( int condition, double valueBegin, double valueEnd )
    {
        const QwtSpline::BoundaryCondition c =
            static_cast< QwtSpline::BoundaryCondition >( condition );

        setBoundaryCondition( QwtSpline::AtBeginning, c );
        setBoundaryValue( QwtSpline::AtEnd, valueBegin );

        setBoundaryCondition( QwtSpline::AtEnd, c );
        setBoundaryValue( QwtSpline::AtEnd, valueEnd );
    }

    QString name() const
    {
        return d_name;
    }

  private:
    const QString d_name;
};

class LocalSpline : public QwtSplineLocal
{
  public:
    LocalSpline( QwtSplineLocal::Type type, const QString& name )
        : QwtSplineLocal( type ),
        d_name( name )
    {
    }

    void setBoundaryConditions( int condition, double valueBegin, double valueEnd )
    {
        const QwtSpline::BoundaryCondition c =
            static_cast< QwtSpline::BoundaryCondition >( condition );

        setBoundaryCondition( QwtSpline::AtBeginning, c );
        setBoundaryValue( QwtSpline::AtEnd, valueBegin );

        setBoundaryCondition( QwtSpline::AtEnd, c );
        setBoundaryValue( QwtSpline::AtEnd, valueEnd );
    }

    QString name() const
    {
        return d_name;
    }

  private:
    const QString d_name;
};

class SplineTester
{
  public:
    enum Type
    {
        Cardinal,
        Akima,
        Cubic
    };

    void testSplineC1( const LocalSpline* spline, const QPolygonF& points ) const;
    void testSplineC2( const CubicSpline* spline, const QPolygonF& points ) const;

  private:
    bool verifyBoundary( const QwtSpline*, QwtSpline::BoundaryPosition,
        const QPolygonF& points, const QVector< double >& m ) const;

    int verifyNodesCV( const QPolygonF& p, const QVector< double >& cv ) const;
    int verifyNodesM( const QPolygonF& p, const QVector< double >& m ) const;
};

void SplineTester::testSplineC1( const LocalSpline* spline, const QPolygonF& points ) const
{
    const QVector< double > m = spline->slopes( points );

    if ( m.size() != points.size() )
    {
        qDebug() << qPrintable( spline->name() ) << "("
                 << points.size() << "):" << "not implemented";
        return;
    }

    const bool okStart = verifyBoundary( spline, QwtSpline::AtBeginning, points, m );
    const bool okEnd = verifyBoundary( spline, QwtSpline::AtEnd, points, m );

    if ( !okStart || !okEnd )
    {
        qDebug() << qPrintable( spline->name() ) << "("
                 << points.size() << "):" << false;

#if DEBUG_ERRORS > 0
        if ( !okStart )
            qDebug() << "  invalid start condition";

        if ( !okEnd )
            qDebug() << "  invalid end condition";
#endif
    }
}

void SplineTester::testSplineC2( const CubicSpline* spline, const QPolygonF& points ) const
{
    const QVector< double > m = spline->slopes( points );
    const QVector< double > cv = spline->curvatures( points );

    if ( m.size() != points.size() )
    {
        qDebug() << qPrintable( spline->name() ) << "("
                 << points.size() << "):" << "not implemented";
        return;
    }

    const bool okStart = verifyBoundary( spline, QwtSpline::AtBeginning, points, m );
    const int numErrorsM = verifyNodesM( points, m );
    const int numErrorsCV = verifyNodesCV( points, cv );
    const bool okEnd = verifyBoundary( spline, QwtSpline::AtEnd, points, m );

    if ( !okStart || numErrorsM > 0 || numErrorsCV > 0 || !okEnd )
    {
        qDebug() << qPrintable( spline->name() ) << "("
                 << points.size() << "):" << false;

#if DEBUG_ERRORS > 0
        if ( !okStart )
            qDebug() << "  invalid start condition";

        if ( numErrorsM > 0 )
            qDebug() << "  invalid node conditions ( slope )" << numErrorsM;

        if ( numErrorsCV > 0 )
            qDebug() << "  invalid node conditions ( curvature )" << numErrorsCV;

        if ( !okEnd )
            qDebug() << "  invalid end condition";
#endif
    }
}

bool SplineTester::verifyBoundary( const QwtSpline* spline, QwtSpline::BoundaryPosition pos,
    const QPolygonF& points, const QVector< double >& m ) const
{
    const bool isC2 = dynamic_cast< const QwtSplineC2* >( spline );

    const int n = points.size();

    const QwtSplinePolynomial polynomBegin = polynomialAt( 0, points, m );
    const QwtSplinePolynomial polynomEnd = polynomialAt( n - 2, points, m );

    bool ok = false;

    if ( spline->boundaryType() != QwtSpline::ConditionalBoundaries )
    {
        // periodic or closed

        const double dx = points[n - 1].x() - points[n - 2].x();

        ok = fuzzyCompare( polynomEnd.slopeAt( dx ),
            polynomBegin.slopeAt( 0.0 ) );

        if ( ok && isC2 )
        {
            ok = fuzzyCompare( polynomEnd.curvatureAt( dx ),
                polynomBegin.curvatureAt( 0.0 ) );
        }

        return ok;
    }

    switch( spline->boundaryCondition( pos ) )
    {
        case QwtSpline::Clamped1:
        {
            const double mt = ( pos == QwtSpline::AtBeginning ) ? m.first() : m.last();
            ok = fuzzyCompare( mt, spline->boundaryValue( pos ) );

            break;
        }
        case QwtSpline::Clamped2:
        {
            double cv;
            if ( pos == QwtSpline::AtBeginning )
            {
                cv = polynomBegin.curvatureAt( 0.0 );
            }
            else
            {
                const double dx = points[n - 1].x() - points[n - 2].x();
                cv = polynomEnd.curvatureAt( dx );
            }

            ok = fuzzyCompare( cv, spline->boundaryValue( pos ) );

            break;
        }
        case QwtSpline::Clamped3:
        {
            double c3;
            if ( pos == QwtSpline::AtBeginning )
                c3 = polynomBegin.c3;
            else
                c3 = polynomEnd.c3;

            ok = fuzzyCompare( 6.0 * c3, spline->boundaryValue( pos ) );
            break;
        }
        case QwtSpline::LinearRunout:
        {
            const double ratio = spline->boundaryValue( pos );
            if ( pos == QwtSpline::AtBeginning )
            {
                const double s = ( points[1].y() - points[0].y() ) /
                    ( points[1].x() - points[0].x() );

                ok = fuzzyCompare( m[0], s - ratio * ( s - m[1] ) );
            }
            else
            {
                const double s = ( points[n - 1].y() - points[n - 2].y() ) /
                    ( points[n - 1].x() - points[n - 2].x() );

                ok = fuzzyCompare( m[n - 1], s - ratio * ( s - m[n - 2] ) );
            }
            break;
        }
        case QwtSplineC2::CubicRunout:
        {
            if ( pos == QwtSpline::AtBeginning )
            {
                const QwtSplinePolynomial polynomBegin2 = polynomialAt( 1, points, m );

                const double cv0 = polynomBegin.curvatureAt( 0.0 );
                const double cv1 = polynomBegin2.curvatureAt( 0.0 );
                const double cv2 = polynomBegin2.curvatureAt( points[2].x() - points[1].x() );

                ok = fuzzyCompare( cv0, 2 * cv1 - cv2 );
            }
            else
            {
                const QwtSplinePolynomial polynomEnd2 = polynomialAt( n - 3, points, m );

                const double cv0 = polynomEnd.curvatureAt( points[n - 1].x() - points[n - 2].x() );
                const double cv1 = polynomEnd.curvatureAt( 0.0 );
                const double cv2 = polynomEnd2.curvatureAt( 0.0 );

                ok = fuzzyCompare( cv0, 2 * cv1 - cv2 );
            }
            break;
        }
        case QwtSplineC2::NotAKnot:
        {
            if ( pos == QwtSpline::AtBeginning )
            {
                const QwtSplinePolynomial polynomBegin2 = polynomialAt( 1, points, m );
                ok = fuzzyCompare( polynomBegin.c3, polynomBegin2.c3 );
            }
            else
            {
                const QwtSplinePolynomial polynomEnd2 = polynomialAt( n - 3, points, m );
                ok = fuzzyCompare( polynomEnd.c3, polynomEnd2.c3 );
            }
            break;
        }
    }

    return ok;
}

int SplineTester::verifyNodesCV( const QPolygonF& points, const QVector< double >& cv ) const
{
    const int n = points.size();

    int numErrors = 0;

    double h1 = points[1].x() - points[0].x();
    double s1 = ( points[1].y() - points[0].y() ) / h1;

    for ( int i = 1; i < n - 2; i++ )
    {
        const double h2 = points[i + 1].x() - points[i].x();
        const double s2 = ( points[i + 1].y() - points[i].y() ) / h2;

        const double v = ( h1 + h2 ) * cv[i] + 0.5 * ( h1 * cv[i - 1] + h2 * cv[i + 1] );
        if ( !fuzzyCompare( v, 3 * ( s2 - s1 ) ) )
        {
#if DEBUG_ERRORS > 1
            qDebug() << "invalid node condition (cv)" << i
                     << cv[i - 1] << cv[i] << cv[i + 1]
                     << v - 3 * ( s2 - s1 );
#endif

            numErrors++;
        }

        h1 = h2;
        s1 = s2;
    }

    return numErrors;
}

int SplineTester::verifyNodesM( const QPolygonF& points, const QVector< double >& m ) const
{
    const int n = points.size();

    int numErrors = 0;

    double dx1 = points[1].x() - points[0].x();
    QwtSplinePolynomial polynomial1 = QwtSplinePolynomial::fromSlopes(
        dx1, points[1].y() - points[0].y(), m[0], m[1] );

    for ( int i = 1; i < n - 1; i++ )
    {
        const double dx2 = points[i + 1].x() - points[i].x();
        const double dy2 = points[i + 1].y() - points[i].y();

        const QwtSplinePolynomial polynomial2 =
            QwtSplinePolynomial::fromSlopes( dx2, dy2, m[i], m[i + 1] );

        const double cv1 = polynomial1.curvatureAt( dx1 );
        const double cv2 = polynomial2.curvatureAt( 0.0 );
        if ( !fuzzyCompare( cv1, cv2 ) )
        {
#if DEBUG_ERRORS > 1
            qDebug() << "invalid node condition (m)" << i <<
                cv1 << cv1 << cv2 - cv1;
#endif

            numErrors++;
        }

        dx1 = dx2;
        polynomial1 = polynomial2;
    }

    return numErrors;
}

static void testSplines( SplineTester::Type splineType, const QPolygonF& points )
{
    struct Condition
    {
        const char* name;
        int condition;
        double valueBegin;
        double valueEnd;
    } conditions[] =
    {
        { "Periodic", -1, 0.0, 0.0 },
        { "Clamped1", QwtSpline::Clamped1, 0.5, 1.0 },
        { "Clamped2", QwtSpline::Clamped2, 0.4, -0.8 },
        { "Clamped3", QwtSpline::Clamped3, 0.03, 0.01 },
        { "Linear Runout", QwtSpline::LinearRunout, 0.3, 0.7 },
        { "Cubic Runout", QwtSplineC2::CubicRunout, 0.0, 0.0 },
        { "Not A Knot", QwtSplineC2::NotAKnot, 0.0, 0.0 }
    };

    for ( uint i = 0; i < sizeof( conditions ) / sizeof( conditions[0] ); i++ )
    {
        const Condition& c = conditions[i];

        if ( conditions[i].condition == QwtSplineC2::NotAKnot && points.size() < 4 )
            continue;

        if ( splineType != SplineTester::Cubic )
        {
            LocalSpline* spline;
            if ( splineType == SplineTester::Cardinal )
            {
                const QString name = QString( c.name ) + " Spline Cardinal";
                spline = new LocalSpline( QwtSplineLocal::Cardinal, name );
            }
            else
            {
                const QString name = QString( c.name ) + " Spline Akima";
                spline = new LocalSpline( QwtSplineLocal::Akima, name );
            }

            if ( c.condition < 0 )
            {
                spline->setBoundaryType( QwtSpline::PeriodicPolygon );
            }
            else
            {
                if ( c.condition == QwtSplineC2::NotAKnot
                    || c.condition == QwtSplineC2::CubicRunout )
                {
                    // requires C2 continuity
                    continue;
                }

                spline->setBoundaryType( QwtSpline::ConditionalBoundaries );
                spline->setBoundaryConditions( c.condition,
                    conditions[i].valueBegin, c.valueEnd );
            }

            SplineTester tester;
            tester.testSplineC1( spline, points );

            delete spline;
        }
        else
        {
            const QString name = QString( c.name ) + " Spline Cubic";

            CubicSpline spline( name );
            if ( conditions[i].condition < 0 )
            {
                spline.setBoundaryType( QwtSpline::PeriodicPolygon );
            }
            else
            {
                spline.setBoundaryType( QwtSpline::ConditionalBoundaries );
                spline.setBoundaryConditions( conditions[i].condition,
                    conditions[i].valueBegin, conditions[i].valueEnd );
            }

            SplineTester tester;
            tester.testSplineC2( &spline, points );
        }
    }
}

static void testSplines( const QPolygonF& points )
{
    testSplines( SplineTester::Cardinal, points );
    testSplines( SplineTester::Akima, points );
    testSplines( SplineTester::Cubic, points );
}

static void testSplines()
{
    QPolygonF points;

    // 3 points

    points << QPointF( 10, 50 ) << QPointF( 60, 30 ) << QPointF( 82, 50 );

    testSplines( points );

    // 4 points

    points.clear();
    points << QPointF( 10, 50 ) << QPointF( 60, 30 )
           << QPointF( 70, 5 ) << QPointF( 82, 50 );

    testSplines( points );

    // 5 points
    points.clear();
    points << QPointF( 10, 50 ) << QPointF( 20, 20 ) << QPointF( 60, 30 )
           << QPointF( 70, 5 ) << QPointF( 82, 50 );

    testSplines( points );

    // 12 points

    points.clear();
    points << QPointF( 10, 50 ) << QPointF( 20, 90 ) << QPointF( 25, 60 )
           << QPointF( 35, 38 ) << QPointF( 42, 40 ) << QPointF( 55, 60 )
           << QPointF( 60, 50 ) << QPointF( 65, 80 ) << QPointF( 73, 30 )
           << QPointF( 82, 30 ) << QPointF( 87, 40 ) << QPointF( 95, 50 );

    testSplines( points );

    // many points
    points.clear();

    const double x1 = 10.0;
    const double x2 = 1000.0;
    const double y1 = -10000.0;
    const double y2 = 10000.0;

    points += QPointF( x1, y1 );

    const int n = 100;
    const double dx = ( x2 - x1 ) / n;
    const int mod = y2 - y1;
    for ( int i = 1; i < n - 1; i++ )
    {
        const double r = qwtRand() % mod;
        points += QPointF( x1 + i * dx, y1 + r );
    }
    points += QPointF( x2, y1 );

    testSplines( points );
}

static void testPaths( const char* prompt, const QwtSpline& spline,
    const QPolygonF& points1, const QPolygonF& points2 )
{
    const QPainterPath path1 = spline.painterPath( points1 );
    const QPainterPath path2 = spline.painterPath( points2 );

    if ( path1 != path2 )
    {
        QString txt( "Parametric Spline" );
        if ( spline.boundaryType() != QwtSpline::ConditionalBoundaries )
            txt += "(closed)";
        txt += ": ";
        txt += prompt;
        txt += " => failed.";

        qDebug() << qPrintable( txt );
    }
}

static void testDuplicates()
{
    QwtSplineLocal spline( QwtSplineLocal::Cardinal );
    spline.setParametrization( QwtSplineParametrization::ParameterChordal );

    QPolygonF points;
    points += QPointF( 1, 6 );
    points += QPointF( 2, 7 );
    points += QPointF( 3, 5);
    points += QPointF( 2, 4 );
    points += QPointF( 0, 3 );

    // inserting duplicates

    QPolygonF points1;
    for ( int i = 0; i < points.size(); i++ )
    {
        points1 += points[i];
        points1 += points[i];
    }

    testPaths( "Duplicates", spline, points, points1 );

    spline.setBoundaryType( QwtSpline::ClosedPolygon );

    testPaths( "Duplicates", spline, points, points1 );

    QPolygonF points2 = points;
    points2.append( points[0] );
    testPaths( "First point also at End", spline, points, points2 );

    QPolygonF points3 = points;
    points3.prepend( points.first() );
    testPaths( "First point twice", spline, points, points3 );

    QPolygonF points4 = points;
    points4.append( points.last() );
    testPaths( "Last point twice", spline, points, points4 );
}

int main()
{
    testSplines();
    testDuplicates();
}
