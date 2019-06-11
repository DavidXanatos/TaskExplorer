#include "stdafx.h"
#include "IncrementalPlot.h"

#include "../../qwt/src/qwt_plot.h"
#include "../../qwt/src/qwt_plot_curve.h"
#include "../../qwt/src/qwt_plot_grid.h"
#include "../../qwt/src/qwt_plot_panner.h"
#include "../../qwt/src/qwt_plot_magnifier.h"
#include "../../qwt/src/qwt_plot_layout.h"
#include "../../qwt/src/qwt_scale_draw.h"
#include "../../qwt/src/qwt_legend.h"
#include "../../qwt/src/qwt_scale_engine.h"

#include <QStaticText>


class QwtPlotEx : public QwtPlot
{
public:
	
	void				SetText(const QString& Text) { m_Text = Text; }
	void				SetTexts(const QStringList& Texts) { m_Texts = Texts; }

protected:
	void drawCanvas(QPainter* p)
	{
		QwtPlot::drawCanvas(p);

		p->setPen(Qt::white); // todo set color properly

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


class CDateScale : public QwtScaleDraw
{
public:
	QwtText label(double value) const { 
		return QDateTime::fromTime_t(value/1000).time().toString(Qt::ISODate);  
	}
};

class CRateScale : public QwtScaleDraw
{
public:
  QwtText label(double value) const {
	  return QString::number(value, 'f', 2);
  }
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

CIncrementalPlot::CIncrementalPlot(const QColor& Back, const QColor& Front, const QColor& Grid, const QString& yAxis, EUnits eUnits, bool useTimer, bool bShowLegent, QWidget *parent)
:QWidget(parent)
{
	m_bReplotPending = false;
	m_iLimit = 300;

	if(useTimer)
	{
		m_Counter = 0;
		m_Timer.start();
	}
	else
		m_Counter = 1;
	

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pPlot = new QwtPlotEx();

	m_pMainLayout->addWidget(m_pPlot);

	setLayout(m_pMainLayout);

	m_pPlot->setStyleSheet(QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6);")
		.arg(Front.red()).arg(Front.green()).arg(Front.blue())
		.arg(Back.red()).arg(Back.green()).arg(Back.blue()));

	QFont smallerFont(QApplication::font());
	m_pPlot->setAxisScaleDraw(QwtPlot::xBottom, new CDateScale);
	switch(eUnits)
	{
	case eBytes: m_pPlot->setAxisScaleDraw(QwtPlot::yLeft, new CRateScale);
	}
	m_pPlot->setAxisFont(QwtPlot::yLeft,smallerFont);
	m_pPlot->setAxisFont(QwtPlot::xBottom,smallerFont);

	m_pPlot->plotLayout()->setAlignCanvasToScales(true);

	if(yAxis.isEmpty())
	{
		m_pPlot->enableAxis(QwtPlot::xBottom, false);
		m_pPlot->enableAxis(QwtPlot::yLeft, false);
	}
	else if(bShowLegent)
	{
		QwtLegendEx* pLegend = new QwtLegendEx;
		pLegend->setStyleSheet(QString("color: rgb(%1, %2, %3);")
			.arg(Front.red()).arg(Front.green()).arg(Front.blue()));
		m_pPlot->insertLegend(pLegend, QwtPlot::BottomLegend);
	}

	if (Grid != Qt::transparent)
	{
		QwtPlotGrid *pGrid = new QwtPlotGrid;
		pGrid->setMajorPen(QPen(Grid, 0, Qt::DotLine));
		pGrid->attach(m_pPlot);
	}

	QwtPlotPanner *pPanner = new QwtPlotPanner(m_pPlot->canvas());
	pPanner->setAxisEnabled(QwtPlot::yRight,false);
	pPanner->setAxisEnabled(QwtPlot::xTop,false);
	pPanner->setMouseButton(Qt::MidButton);
	
	m_pPlot->setAxisAutoScale(QwtPlot::yLeft);
	m_pPlot->setAxisAutoScale(QwtPlot::xBottom);
	m_pPlot->setAxisTitle(QwtPlot::yLeft,yAxis);
	//m_pPlot->setAxisTitle(QwtPlot::xBottom,tr("Time"));
}

CIncrementalPlot::~CIncrementalPlot()
{
	Clear();
}

void CIncrementalPlot::SetLimit(int iLimit)
{
	m_iLimit = iLimit; 
}

void CIncrementalPlot::SetRagne(double Max, double Min)
{
	m_pPlot->setAxisScale(QwtPlot::yLeft, Min, Max);
}

double CIncrementalPlot::GetRangeMax()
{
	return m_pPlot->axisScaleDiv(QwtPlot::yLeft).upperBound();
}

void CIncrementalPlot::SetText(const QString& Text)
{
	m_pPlot->SetText(Text);
}

void CIncrementalPlot::SetTexts(const QStringList& Texts)
{
	m_pPlot->SetTexts(Texts);
}

void CIncrementalPlot::Clear()
{
	foreach(const SCurve& Curve, m_Curves)
	{
		delete Curve.pPlot;
		free(Curve.xData);
		free(Curve.yData);
	}
	m_Curves.clear();
}

void CIncrementalPlot::AddPlot(const QString& Name, const QColor& Color, Qt::PenStyle Style, bool Fill, const QString& Title, int width)
{
	SCurve& Curve = m_Curves[Name];

	Curve.pPlot = new QwtPlotCurve(Title);
	Curve.pPlot->setPen(QPen(QBrush(Color), width, Style));
	
	if (Fill)
	{
		Curve.pPlot->setBaseline(0);
		Curve.pPlot->setBrush(QBrush(Color));
	}

	if (m_Counter && m_iLimit)
	{
		Curve.uSize = m_iLimit;
		Curve.xData = (double*)malloc(sizeof(double)*Curve.uSize);
		Curve.yData = (double*)malloc(sizeof(double)*Curve.uSize);

		for (int i = 0; i < Curve.uSize; i++)
		{
			Curve.xData[i] = i;
			Curve.yData[i] = 0;
		}
	}
	else
	{
		Curve.uSize = 1;
		Curve.xData = (double*)malloc(sizeof(double)*Curve.uSize);
		Curve.yData = (double*)malloc(sizeof(double)*Curve.uSize);
		Curve.xData[Curve.uSize - 1] = m_Counter ? m_Counter++ : m_Timer.elapsed();
		Curve.yData[Curve.uSize - 1] = 0;
	}

	Curve.pPlot->setRawSamples(Curve.xData,Curve.yData,Curve.uSize);
    Curve.pPlot->attach(m_pPlot);

    /*zoomer = new ChartDateZoomer(qpChart->canvas());
    zoomer->setRubberBandPen(QPen(Qt::red,2,Qt::DotLine));
    zoomer->setTrackerPen(QPen(Qt::red));
    zoomer->setMousePattern(QwtEventPattern::MouseSelect2,Qt::RightButton,Qt::ControlModifier);
    zoomer->setMousePattern(QwtEventPattern::MouseSelect3,Qt::RightButton);*/
}

void CIncrementalPlot::AddPlotPoint(const QString& Name, double Value)
{
	if(!m_Curves.contains(Name))
		return;

	SCurve& Curve = m_Curves[Name];

	bool bFreezeX = false;
	if(m_iLimit > Curve.uSize)
	{
		Curve.uSize++;
		Curve.xData = (double*)realloc(Curve.xData, sizeof(double)*Curve.uSize);
		Curve.yData = (double*)realloc(Curve.yData, sizeof(double)*Curve.uSize);
	}
	else
	{
		if (m_Counter)
			bFreezeX = true;
		else
			memmove(Curve.xData, Curve.xData + 1, sizeof(double)*(Curve.uSize - 1));
		memmove(Curve.yData, Curve.yData + 1, sizeof(double)*(Curve.uSize - 1));
	}
	if (!bFreezeX)
		Curve.xData[Curve.uSize-1] = m_Counter ? m_Counter++ : m_Timer.elapsed();
	Curve.yData[Curve.uSize-1] = Value;

	Curve.pPlot->setRawSamples(Curve.xData,Curve.yData,Curve.uSize);

	if(!m_bReplotPending)
	{
		m_bReplotPending = true;
		QTimer::singleShot(100,this,SLOT(Replot()));
	}
}

void CIncrementalPlot::Replot()
{
	m_pPlot->replot();
	m_bReplotPending = false;
}

void CIncrementalPlot::Reset()
{
	if(m_Counter)
		m_Counter = 1;

	foreach(const SCurve& curve, m_Curves)
	{
		SCurve& Curve = *(SCurve*)&curve;

		Curve.uSize = 1;
		Curve.xData = (double*)malloc(sizeof(double)*Curve.uSize);
		Curve.yData = (double*)malloc(sizeof(double)*Curve.uSize);
		Curve.xData[Curve.uSize-1] = m_Counter ? m_Counter++ : m_Timer.elapsed();
		Curve.yData[Curve.uSize-1] = 0;

		Curve.pPlot->setRawSamples(Curve.xData,Curve.yData,Curve.uSize);
	}

	m_pPlot->replot();
}

