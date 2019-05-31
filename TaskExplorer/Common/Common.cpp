#include "stdafx.h"
#include "Common.h"


QString FormatSize(quint64 Size)
{
	double Div;
	if(Size > (quint64)(Div = 1.0*1024*1024*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "EB";
	if(Size > (quint64)(Div = 1.0*1024*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "PB";
	if(Size > (quint64)(Div = 1.0*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "TB";
	if(Size > (quint64)(Div = 1.0*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "GB";
	if(Size > (quint64)(Div = 1.0*1024*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "MB";
	if(Size > (quint64)(Div = 1.0*1024))
		return QString::number(double(Size)/Div, 'f', 2) + "kB";
	return QString::number(double(Size)) + "B";
}

QString FormatTime(quint64 Time)
{
	int seconds = Time % 60;
	Time /= 60;
	int minutes = Time % 60;
	Time /= 60;
	int hours = Time % 24;
	int days = Time / 24;
	if((hours == 0) && (days == 0))
		return QString().sprintf("%02d:%02d", minutes, seconds);
	if (days == 0)
		return QString().sprintf("%02d:%02d:%02d", hours, minutes, seconds);
	return QString().sprintf("%dd%02d:%02d:%02d", days, hours, minutes, seconds);
}

void GrayScale (QImage& Image)
{
	if (Image.depth () == 32)
	{
		uchar* r = (Image.bits ());
		uchar* g = (Image.bits () + 1);
		uchar* b = (Image.bits () + 2);

		uchar* end = (Image.bits() + Image.byteCount ());
		while (r != end)
		{
			*r = *g = *b = (((*r + *g) >> 1) + *b) >> 1; // (r + b + g) / 3

			r += 4;
			g += 4;
			b += 4;
		}
	}
	else
	{
		for (int i = 0; i < Image.colorCount (); i++)
		{
			uint r = qRed (Image.color (i));
			uint g = qGreen (Image.color (i));
			uint b = qBlue (Image.color (i));

			uint gray = (((r + g) >> 1) + b) >> 1;

			Image.setColor (i, qRgba (gray, gray, gray, qAlpha (Image.color (i))));
		}
	}
}

QAction* MakeAction(QToolBar* pParent, const QString& IconFile, const QString& Text)
{
	QAction* pAction = new QAction(Text, pParent);
	
	QImage Image(IconFile);
	QIcon Icon;
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
	GrayScale(Image);
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
	pAction->setIcon(Icon);
	
	pParent->addAction(pAction);
	return pAction;
}

QMenu* MakeMenu(QMenu* pParent, const QString& Text, const QString& IconFile)
{
	if(!IconFile.isEmpty())
	{
		QImage Image(IconFile);
		QIcon Icon;
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
		GrayScale(Image);
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
		return pParent->addMenu(Icon, Text);
	}
	return pParent->addMenu(Text);
}

QAction* MakeAction(QMenu* pParent, const QString& Text, const QString& IconFile)
{
	QAction* pAction = new QAction(Text, pParent);
	if(!IconFile.isEmpty())
	{
		QImage Image(IconFile);
		QIcon Icon;
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
		GrayScale(Image);
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
		pAction->setIcon(Icon);
	}
	pParent->addAction(pAction);
	return pAction;
}

QAction* MakeAction(QActionGroup* pGroup, QMenu* pParent, const QString& Text, const QVariant& Data)
{
	QAction* pAction = new QAction(Text, pParent);
	pAction->setCheckable(true);
	pAction->setData(Data);
	pAction->setActionGroup(pGroup);
	pParent->addAction(pAction);
	return pAction;
}