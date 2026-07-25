#pragma once
#include "../mischelpers_global.h"

class MISCHELPERS_EXPORT CCustomTheme//: QObject
{
	//Q_OBJECT
public:
	CCustomTheme();// (QObject* pParent = 0);

	static bool IsDarkTheme() { return m_DarkTheme; }
	bool IsSystemDark();

	void SetUITheme(bool bDark, int iFusion = 2);

#if defined(Q_OS_WIN)
	void SetTitleTheme(const HWND& hwnd);
#endif

private:
	QString				m_DefaultStyle;
	QPalette			m_DefaultPalett;
	QPalette			m_DarkPalett;

	static bool			m_DarkTheme;
};

void MISCHELPERS_EXPORT FixTriStateBoxPallete(QWidget* pWidget);