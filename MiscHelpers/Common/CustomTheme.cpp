#include "stdafx.h"
#include "CustomTheme.h"
#include "CustomStyles.h"
#include "TreeItemModel.h"
#include "PanelView.h"
#include <qsettings.h>

#include <wtypes.h>

bool CCustomTheme::m_DarkTheme = false;


CCustomTheme::CCustomTheme()//(QObject * pParent)
	//: QObject(pParent)
{
	m_DefaultStyle = QApplication::style()->objectName();
	m_DefaultPalett = QApplication::palette();

	m_DarkPalett.setColor(QPalette::Light, QColor(96, 96, 96));
	m_DarkPalett.setColor(QPalette::Midlight, QColor(64, 64, 64));
	m_DarkPalett.setColor(QPalette::Mid, QColor(48, 48, 48));
	m_DarkPalett.setColor(QPalette::Dark, QColor(53, 53, 53));
	m_DarkPalett.setColor(QPalette::Shadow, QColor(25, 25, 25));
	m_DarkPalett.setColor(QPalette::Window, QColor(53, 53, 53));
	m_DarkPalett.setColor(QPalette::WindowText, Qt::white);
	m_DarkPalett.setColor(QPalette::Base, QColor(25, 25, 25));
	m_DarkPalett.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
	m_DarkPalett.setColor(QPalette::ToolTipBase, Qt::lightGray);
	m_DarkPalett.setColor(QPalette::ToolTipText, Qt::white);
	m_DarkPalett.setColor(QPalette::Text, Qt::white);
	m_DarkPalett.setColor(QPalette::Button, QColor(53, 53, 53));
	m_DarkPalett.setColor(QPalette::ButtonText, Qt::white);
	m_DarkPalett.setColor(QPalette::BrightText, Qt::red);
	m_DarkPalett.setColor(QPalette::Link, QColor(218, 130, 42));
	m_DarkPalett.setColor(QPalette::LinkVisited, QColor(218, 130, 42));
	m_DarkPalett.setColor(QPalette::Highlight, QColor(42, 130, 218));
	m_DarkPalett.setColor(QPalette::HighlightedText, Qt::black);
	m_DarkPalett.setColor(QPalette::PlaceholderText, QColor(96, 96, 96));
	m_DarkPalett.setColor(QPalette::Disabled, QPalette::WindowText, Qt::darkGray);
	m_DarkPalett.setColor(QPalette::Disabled, QPalette::Text, Qt::darkGray);
	m_DarkPalett.setColor(QPalette::Disabled, QPalette::Light, Qt::black);
	m_DarkPalett.setColor(QPalette::Disabled, QPalette::ButtonText, Qt::darkGray);
}	

bool CCustomTheme::IsSystemDark()
{
	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
	bool bDark = (settings.value("AppsUseLightTheme") == 0);
	return bDark;
}

#if defined(Q_OS_WIN)
void CCustomTheme::SetTitleTheme(const HWND& hwnd)
{
	static const int CurrentVersion = QSettings("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
		QSettings::NativeFormat).value("CurrentBuild").toInt();
	if (CurrentVersion < 17763) // Windows 10 1809 -
		return;

	HMODULE dwmapi = GetModuleHandleW(L"dwmapi.dll");
	if (dwmapi)
	{
		typedef HRESULT(WINAPI* P_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
		P_DwmSetWindowAttribute pDwmSetWindowAttribute = reinterpret_cast<P_DwmSetWindowAttribute>(GetProcAddress(dwmapi, "DwmSetWindowAttribute"));
		if (pDwmSetWindowAttribute)
		{
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
			BOOL Dark = m_DarkTheme;
			pDwmSetWindowAttribute(hwnd,
				CurrentVersion >= 18985 ? DWMWA_USE_IMMERSIVE_DARK_MODE : DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1,
				&Dark, sizeof(Dark));
		}
	}
}
#endif

void CCustomTheme::SetUITheme(bool bDark, int iFusion)
{
	if (bDark)
		QApplication::setPalette(m_DarkPalett);
	else
		QApplication::setPalette(m_DefaultPalett);
	m_DarkTheme = bDark;


	bool bFusion;
	if (iFusion == 2)
		bFusion = bDark;
	else 
		bFusion = (iFusion == 1);

	if (bFusion)
		QApplication::setStyle(QStyleFactory::create("Fusion"));
	else
		QApplication::setStyle(QStyleFactory::create(bDark ? "Windows" : m_DefaultStyle));
	QApplication::setStyle(new KeepSubMenusVisibleStyle(new CustomTabStyle(QApplication::style())));


	CTreeItemModel::SetDarkMode(bDark);
	//CListItemModel::SetDarkMode(bDark); // not used
	CPanelView::SetDarkMode(bDark);
	CFinder::SetDarkMode(bDark);


#if defined(Q_OS_WIN)
	foreach(QWidget * pWidget, QApplication::topLevelWidgets())
	{
		if (pWidget->isVisible())
			SetTitleTheme((HWND)pWidget->winId());
	}
#endif
}


void FixTriStateBoxPallete(QWidget* pWidget)
{
	if (QApplication::style()->objectName() == "windows") {

		//
		// the built in "windows" theme of Qt does not properly rendered PartiallyChecked
		// checkboxes, to remedi this issue we connect to the stateChanged slot
		// and change the pattern to improve the rendering.
		//

		foreach(QCheckBox* pCheck, pWidget->findChildren<QCheckBox*>()) {
			QObject::connect(pCheck, &QCheckBox::stateChanged, [pCheck](int state) {
				if (pCheck->isTristate()) {
					QPalette palette = QApplication::palette();
					if (state == Qt::PartiallyChecked)
						palette.setColor(QPalette::Base, Qt::darkGray);
					pCheck->setPalette(palette);
				}
			});
		}
	}
}