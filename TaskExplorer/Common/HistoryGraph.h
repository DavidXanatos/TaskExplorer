#pragma once

class CHistoryGraph: public QWidget
{
public:
	CHistoryGraph(bool bSimpleMode = false, QWidget* parent = 0) : QWidget(parent) {
		m_SimpleMode = bSimpleMode;
	}
	~CHistoryGraph() {}

	void AddValue(int Id, QColor Color)
	{
		m_Values.insert(Id, { 0.0, Color });
	}

	void SetValue(int Id, float Value)
	{
		m_Values[Id].Value = Value;
	}

	void Update(int CellHeight, int CellWidth)
	{
		// init / resize
		if(m_Graph.height() != CellWidth /*|| m_Graph.width() != curHeight*/)
		{
			QImage Graph = QImage(CellHeight, CellWidth, QImage::Format_RGB32);
			QPainter qp(&Graph);
			qp.fillRect(-1, -1, CellHeight+1, CellWidth+1, Qt::white);
			if (!m_Graph.isNull())
				qp.drawImage(0, Graph.height() - m_Graph.height(), m_Graph);
			m_Graph = Graph;
		}

		// shift
		uchar *dest = m_Graph.bits();
		int pos = m_Graph.sizeInBytes() - m_Graph.bytesPerLine();
		memmove(dest, dest + m_Graph.bytesPerLine(), pos);
		QPainter qp(&m_Graph);

		// draw new data points
		int top = 0;

#if 0
		foreach(const SValue& Value, m_Values)
		{
			int x = (float)(m_Graph.width()) * Value.Value;
			if (x > top)
				top = x;

			qp.setPen(Value.Color);
			qp.drawLine(0, m_Graph.height() - 1, x, m_Graph.height());
		}
			
		qp.setPen(Qt::white);
		qp.drawLine(top, m_Graph.height() - 1, m_Graph.width()-1, m_Graph.height());
#else
		dest += pos;
		memset(dest, 0, m_Graph.bytesPerLine()); // fill line black
		ASSERT(m_Graph.depth() == 32);
		int max = m_Graph.width();
		//ASSERT(max * 4 == m_Graph.bytesPerLine());
		
		foreach(const SValue& Value, m_Values)
		{
			int x = (float)(max) * Value.Value;
			if (x > max)
				x = max;
			if (x > top)
				top = x;
			x *= 4;

			int r, g, b;
			Value.Color.getRgb(&r, &g, &b);
			

			for (int i = 0; i < x; i += 4)
			{
				if (m_SimpleMode || *(quint32*)(dest + i) == 0)
				{
					dest[i    ] = b;
					dest[i + 1] = g;
					dest[i + 2] = r;
				}
				else
				{
#define DIV 4/5
					int cb = (int)dest[i    ]*DIV + b*DIV;
					int cg = (int)dest[i + 1]*DIV + g*DIV;
					int cr = (int)dest[i + 2]*DIV + r*DIV;

					dest[i    ] = qMin(cb, 255);
					dest[i + 1] = qMin(cg, 255);
					dest[i + 2] = qMin(cr, 255);
				}
			}
		}

		top *= 4;
		if (top < m_Graph.bytesPerLine()) // fill rest white
			memset(dest + top, 0xFF, m_Graph.bytesPerLine() - top);
#endif
	}

protected:
	void paintEvent(QPaintEvent* e)
	{
		QPainter qp(this);
		qp.translate(width() - m_Graph.height()-1, height());
		qp.rotate(270);
		qp.drawImage(0, 0, m_Graph);
	}

	QImage	m_Graph;

	struct SValue
	{
		float Value;
		QColor Color;
	};
	QMap<int, SValue>	m_Values;
	bool				m_SimpleMode;
};