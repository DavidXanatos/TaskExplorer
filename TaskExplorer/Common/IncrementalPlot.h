#pragma once

#include <QWidget>
#include <QDateTime>

#include "../../qwt/src/qwt_plot.h"
#include "../../qwt/src/qwt_plot_curve.h"
#include "../../qwt/src/qwt_plot_grid.h"
#include "../../qwt/src/qwt_plot_panner.h"
#include "../../qwt/src/qwt_plot_magnifier.h"
#include "../../qwt/src/qwt_plot_layout.h"
#include "../../qwt/src/qwt_scale_draw.h"
#include "../../qwt/src/qwt_legend.h"
#include "../../qwt/src/qwt_scale_engine.h"
#include "../../qwt/src/qwt_plot_zoomer.h"

#include <QStaticText>

class QwtPlotEx : public QwtPlot
{
public:
	
	void SetText(const QString& Text) { m_Text = Text; }
	void SetTexts(const QStringList& Texts) { m_Texts = Texts; }

protected:
	void drawCanvas(QPainter* p)
	{
		QwtPlot::drawCanvas(p);

		p->setPen(Qt::white); // todo: set color from proeprty

		int x = 2;
		int y = 0;

		QStaticText Line(m_Text);
		p->drawStaticText (x, y, Line);
		y += Line.size().height();
		
		for (int i = 0; i < m_Texts.count(); i++)
		{
			QwtPlotCurve* pCurve = (QwtPlotCurve*)this->itemList()[i];
			p->setPen(pCurve->pen()); 

			Line.setText(m_Texts[i]);
			p->drawStaticText (x, y, Line);
			x += 4 + Line.size().width();
		}
	}

	QString m_Text;
	QStringList m_Texts;
};


class QwtLegendEx: public QwtLegend
{
public:
	QwtLegendEx() {}

    virtual void updateLegend( const QVariant &itemInfo, const QList<QwtLegendData> &data )
	{
		QList<QwtLegendData> temp = data;
		for(int i=0; i < temp.size(); i++)
		{
			if(temp[i].title().isEmpty())
				temp.removeAt(i--);
		}
		QwtLegend::updateLegend(itemInfo, temp);
	}
};


/*
class CPlotInfo : public QwtPlotPicker
{
public:
	CPlotInfo(int xAxis, int yAxis, QWidget *canvas) :
		QwtPlotPicker(xAxis, yAxis, canvas)
	{
		setRubberBand(QwtPicker::RectRubberBand);
		setRubberBandPen(QColor(Qt::green));
		//setTrackerMode(QwtPicker::ActiveOnly);
		setTrackerMode(QwtPlotPicker::AlwaysOn);
		setTrackerPen(QColor(Qt::white));
	}

protected:
	virtual QwtText trackerTextF(const QPointF &pos) const
	{
		QDateTime dt = QDateTime::fromTime_t(pos.x());

		//QString s = QString(" %1 @ %2 \r\n test\r\n test ").arg(pos.y()).arg(dt.toString("dd-MMM-yyyy hh:mm"));

		QString s = "test <b>test</b><br><FONT color='#ffffff'>test sdsadasdasdsa s asdds as sasd</FONT><br>test<br>test";

		QwtText text(s, QwtText::RichText);
		text.setColor(Qt::black);

		text.setBorderPen(QPen(Qt::darkBlue, 2));
		//text.setBorderRadius(6);
		text.setBackgroundBrush(Qt::gray);

		return text;
	}

	QRect trackerRect( const QFont &font ) const
	{
		if ( trackerMode() == AlwaysOff ||
			( trackerMode() == ActiveOnly && !isActive() ) )
		{
			return QRect();
		}

		QPoint trackerPosition = this->trackerPosition();
		QPolygon pickedPoints = this->pickedPoints();

		if ( trackerPosition.x() < 0 || trackerPosition.y() < 0 )
			return QRect();

		QwtText text = trackerText( trackerPosition );
		if ( text.isEmpty() )
			return QRect();

		const QSizeF textSize = text.textSize( font );
		QRect textRect( 0, 0, qCeil( textSize.width() ), qCeil( textSize.height() ) );

		const QPoint &pos = trackerPosition;

		int alignment = 0;
		if ( isActive() && pickedPoints.count() > 1
			&& rubberBand() != NoRubberBand )
		{
			const QPoint last = pickedPoints[int( pickedPoints.count() ) - 2];

			alignment |= ( pos.x() >= last.x() ) ? Qt::AlignRight : Qt::AlignLeft;
			alignment |= ( pos.y() > last.y() ) ? Qt::AlignBottom : Qt::AlignTop;
		}
		else
			alignment = Qt::AlignTop | Qt::AlignRight;

		const int margin = 10;

		int x = pos.x();
		if ( alignment & Qt::AlignLeft )
			x -= textRect.width() + margin;
		else if ( alignment & Qt::AlignRight )
			x += margin;

		int y = pos.y();
		if ( alignment & Qt::AlignBottom )
			y += margin;
		else if ( alignment & Qt::AlignTop )
			y -= textRect.height() + margin;

		textRect.moveTopLeft( QPoint( x, y ) );

		const QRect pickRect = pickArea().boundingRect().toRect();

		int right = qMin( textRect.right(), pickRect.right() - margin );
		int bottom = qMin( textRect.bottom(), pickRect.bottom() - margin );
		textRect.moveBottomRight( QPoint( right, bottom ) );

		int left = qMax( textRect.left(), pickRect.left() + margin );
		int top = qMax( textRect.top(), pickRect.top() + margin );
		textRect.moveTopLeft( QPoint( left, top ) );

		return textRect;
	}
};

*/


class CIncrementalPlot : public QWidget
{
	Q_OBJECT

public:
	enum EUnits {
		eAU,
		eBytes,
		eDate
	};

	CIncrementalPlot(const QColor& Back = Qt::darkGray, const QColor& Front = Qt::transparent, const QColor& Grid = Qt::transparent, QWidget *parent = 0);
	~CIncrementalPlot();

	void				UseTimer(bool bUse = true);

	void				SetupLegend(const QColor& Front = Qt::lightGray, const QString& yAxis = "", EUnits eXUnits = eAU, EUnits eYUnits = eAU, bool useTimer = false, bool bShowLegent = true);

	void				SetRagne(double Max, double Min = 0.0);
	double				GetRangeMax();
	void				AddPlot(const QString& Name, const QColor& Color, Qt::PenStyle Style, bool Fill = false, const QString& Title = "", int width = 1);
	void				RemovePlot(const QString& Name);
	void				AddPlotPoint(const QString& Name, double Value);

	void				SetText(const QString& Text);
	void				SetTexts(const QStringList& Texts);

	void				Reset();
	void				Clear();

	void				SetLimit(int iLimit);

	QwtPlotEx*			GetPlot() { return m_pPlot; }

signals:
	//void				Entered();
	//void				Moveed(QMouseEvent* event);
	//void				Exited();
	void				ToolTipRequested(QEvent* event);

public slots:
	void				Replot();

protected:
	bool event(QEvent *event) override;

	struct SCurve
	{
		SCurve() : pPlot(0), uSize(0), xData(0), yData(0) {}
		QwtPlotCurve*	pPlot;
		size_t			uSize;
		double*			xData;
		double*			yData;
	};

	void				InitCurve(SCurve& Curve);

	QVBoxLayout*		m_pMainLayout;

	QwtPlotEx*			m_pPlot;
	QMap<QString, SCurve> m_Curves;
	bool				m_bReplotPending;
	int					m_iLimit;

	QElapsedTimer		m_Timer;
	quint64				m_Counter;
	bool				m_UseDate;
};
