#pragma once

#include <QWidget>
#include <QDateTime>

class QwtPlotEx;
class QwtPlotCurve;

class CIncrementalPlot : public QWidget
{
  Q_OBJECT

public:
	enum EUnits{
		eAU,
		eBytes
	};

	CIncrementalPlot(const QColor& Back = Qt::darkGray, const QColor& Front = Qt::lightGray, const QColor& Grid = Qt::transparent, const QString& yAxis = "", EUnits eUnits = eAU, bool useTimer = false, bool bShowLegent = true, QWidget *parent = 0);
	~CIncrementalPlot();

	void				SetRagne(double Max, double Min = 0.0);
	double				GetRangeMax();
	void				AddPlot(const QString& Name, const QColor& Color, Qt::PenStyle Style, bool Fill = false, const QString& Title = "", int width = 1);
	void				AddPlotPoint(const QString& Name, double Value);

	void				SetText(const QString& Text);
	void				SetTexts(const QStringList& Texts);

	void				Reset();
	void				Clear();

	void				SetLimit(int iLimit);

public slots:
	void				Replot();

protected:
	QVBoxLayout*		m_pMainLayout;

	QwtPlotEx*			m_pPlot;
	struct SCurve
	{
		SCurve() : pPlot(0), uSize(0), xData(0), yData(0) {}
		QwtPlotCurve*	pPlot;
		size_t			uSize;
		double*			xData;
		double*			yData;
	};
	QMap<QString, SCurve> m_Curves;
	bool				m_bReplotPending;
	int					m_iLimit;

	QElapsedTimer		m_Timer;
	quint64				m_Counter;
};
