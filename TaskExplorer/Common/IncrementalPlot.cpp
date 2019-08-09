#include "stdafx.h"
#include "IncrementalPlot.h"
#include "Common.h"

class CDateScale : public QwtScaleDraw
{
public:
	QwtText label(double value) const { 
		return QDateTime::fromTime_t(value).time().toString(Qt::ISODate);  
	}
};

class CBytesScale : public QwtScaleDraw
{
public:
  QwtText label(double value) const {
	  return FormatSize(value);
  }
};

CIncrementalPlot::CIncrementalPlot(const QColor& Back, const QColor& Front, const QColor& Grid, QWidget *parent)
	:QWidget(parent)
{
	m_bReplotPending = false;
	m_iLimit = 300;

	m_Counter = 1;
	m_UseDate = false;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);

	m_pPlot = new QwtPlotEx();

	m_pMainLayout->addWidget(m_pPlot);

	setLayout(m_pMainLayout);

	if (Front == Qt::transparent)
	{
		m_pPlot->setStyleSheet(QString("background-color: rgb(%4, %5, %6);")
			.arg(Back.red()).arg(Back.green()).arg(Back.blue()));

		m_pPlot->enableAxis(QwtPlot::xBottom, false);
		m_pPlot->enableAxis(QwtPlot::yLeft, false);
	}
	else
	{
		m_pPlot->setStyleSheet(QString("color: rgb(%1, %2, %3); background-color: rgb(%4, %5, %6);")
			.arg(Front.red()).arg(Front.green()).arg(Front.blue())
			.arg(Back.red()).arg(Back.green()).arg(Back.blue()));
	}

	if (Grid != Qt::transparent)
	{
		QwtPlotGrid *pGrid = new QwtPlotGrid;
		pGrid->setMajorPen(QPen(Grid, 0, Qt::DotLine));
		pGrid->attach(m_pPlot);
	}

	/*QwtPlotPanner *pPanner = new QwtPlotPanner(m_pPlot->canvas());
	pPanner->setAxisEnabled(QwtPlot::yRight,false);
	pPanner->setAxisEnabled(QwtPlot::xTop,false);
	pPanner->setMouseButton(Qt::MidButton);*/
	
	m_pPlot->setAxisAutoScale(QwtPlot::yLeft);
	m_pPlot->setAxisAutoScale(QwtPlot::xBottom);

	//new CPlotInfo(QwtPlot::xBottom, QwtPlot::yLeft, m_pPlot->canvas());

    /*zoomer = new ChartDateZoomer(qpChart->canvas());
    zoomer->setRubberBandPen(QPen(Qt::red,2,Qt::DotLine));
    zoomer->setTrackerPen(QPen(Qt::red));
    zoomer->setMousePattern(QwtEventPattern::MouseSelect2,Qt::RightButton,Qt::ControlModifier);
    zoomer->setMousePattern(QwtEventPattern::MouseSelect3,Qt::RightButton);*/
}

CIncrementalPlot::~CIncrementalPlot()
{
	Clear();
}

void CIncrementalPlot::UseTimer(bool bUse)
{
	if(bUse)
	{
		m_Counter = 0;
		m_Timer.start();
	}
	else
		m_Counter = 1;
}

void CIncrementalPlot::SetupLegend(const QColor& Front, const QString& yAxis, EUnits eXUnits, EUnits eYUnits, bool useTimer, bool bShowLegent)
{
	QFont smallerFont(QApplication::font());
	switch (eXUnits)
	{
		case eDate:		
			m_pPlot->setAxisScaleDraw(QwtPlot::xBottom, new CDateScale); 
			break;
	}
	switch(eYUnits)
	{
		case eBytes:	
			m_pPlot->setAxisScaleDraw(QwtPlot::yLeft, new CBytesScale);
			break;
	}
	m_UseDate = (eXUnits == eDate);
	m_pPlot->setAxisFont(QwtPlot::yLeft,smallerFont);
	m_pPlot->setAxisFont(QwtPlot::xBottom,smallerFont);

	m_pPlot->plotLayout()->setAlignCanvasToScales(true);

	m_pPlot->enableAxis(QwtPlot::xBottom, !yAxis.isEmpty());
	m_pPlot->enableAxis(QwtPlot::yLeft, !yAxis.isEmpty());

	if (bShowLegent)
	{
		QwtLegendEx* pLegend = new QwtLegendEx;
		pLegend->setStyleSheet(QString("color: rgb(%1, %2, %3);")
			.arg(Front.red()).arg(Front.green()).arg(Front.blue()));
		m_pPlot->insertLegend(pLegend, QwtPlot::BottomLegend);
	}

	m_pPlot->setAxisTitle(QwtPlot::yLeft,yAxis);
	//m_pPlot->setAxisTitle(QwtPlot::xBottom,tr("Time"));
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
	m_pPlot->setAxisAutoScale(QwtPlot::yLeft);

	foreach(const SCurve& Curve, m_Curves)
	{
		delete Curve.pPlot;
		free(Curve.xData);
		free(Curve.yData);
	}
	m_Curves.clear();
}

void CIncrementalPlot::InitCurve(SCurve& Curve)
{
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

	if (!m_UseDate)
		InitCurve(Curve);

    Curve.pPlot->attach(m_pPlot);

	m_pPlot->canvas()->setCursor(Qt::ArrowCursor);
}

void CIncrementalPlot::RemovePlot(const QString& Name)
{
	if(!m_Curves.contains(Name))
		return;

	SCurve& Curve = m_Curves.take(Name);
	
	delete Curve.pPlot;
	free(Curve.xData);
	free(Curve.yData);
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
		if (m_Counter && !m_UseDate)
			bFreezeX = true;
		else
			memmove(Curve.xData, Curve.xData + 1, sizeof(double)*(Curve.uSize - 1));
		memmove(Curve.yData, Curve.yData + 1, sizeof(double)*(Curve.uSize - 1));
	}

	if (m_UseDate)
		Curve.xData[Curve.uSize-1] = GetTime();
	else if (!bFreezeX)
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

		if (Curve.xData) {
			free(Curve.xData);
			Curve.xData = NULL;
		}
		if (Curve.yData) {
			free(Curve.yData);
			Curve.yData = NULL;
		}
		Curve.uSize = 0;

		if (!m_UseDate)
			InitCurve(Curve);
	}

	m_pPlot->replot();
}

bool CIncrementalPlot::event(QEvent *event)
{
    switch ( event->type() )
    {
        case QEvent::Resize:    
            break;
        case QEvent::Enter:
			//emit Entered();
            break;
        case QEvent::Leave:
			//emit Exited();
            break;
        case QEvent::MouseButtonPress:
            break;
        case QEvent::MouseButtonRelease:
            break;
        case QEvent::MouseButtonDblClick:
            break;
        case QEvent::MouseMove:
			//emit Moveed((QMouseEvent*)event);
            break;
        case QEvent::KeyPress:
            break;
        case QEvent::KeyRelease:
            break;
        case QEvent::Wheel:    
            break;
		case QEvent::ToolTip:
		{
			emit ToolTipRequested(event);
			return true;
		}
        default:
			break;
    }
	return QWidget::event(event);
}