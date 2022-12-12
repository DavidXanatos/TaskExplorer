#include "stdafx.h"
#include "WinModuleFinder.h"
#include "../ProcessHacker.h"
#include "../WinModule.h"
#include "../WindowsAPI.h"


CWinModuleFinder::CWinModuleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent) : CAbstractFinder(parent)
{
	m_Type = Type;
	m_RegExp = RegExp;
}

CWinModuleFinder::~CWinModuleFinder() 
{
}

void CWinModuleFinder::run()
{
	bool bIncludeMappedFiles = m_Type.toInt() == 0 || m_Type.toInt() == -1;

	QList<QSharedPointer<QObject>> List;

	quint64 TimeStamp = GetTime() * 1000;

	QMap<quint64, CProcessPtr> Processes = theAPI->GetProcessList();
	int Modulo = Processes.count() / 100;
	int i = 0;
	for (QMap<quint64, CProcessPtr>::iterator I = Processes.begin(); I != Processes.end() && !m_bCancel; ++I)
	{
		if (Modulo && (i++ % Modulo) == 0)
			emit Progress(float(i) / Processes.count(), I.value()->GetName());

		if (I.value()->UpdateModules())
		{
			quint64 FirstBaseAddress = 0;

			QMap<quint64, CModulePtr> Modules = I.value()->GetModuleList();
			for (QMap<quint64, CModulePtr>::iterator J = Modules.begin(); J != Modules.end() && !m_bCancel; ++J)
			{
				if (!bIncludeMappedFiles && J.value().staticCast<CWinModule>()->GetType() == PH_MODULE_TYPE_MAPPED_FILE)
					continue;

				if (!J.value()->GetFileName().contains(m_RegExp))
					continue;
				
				J.value()->m_pProcess = I.value();

				List.append(J.value());
			}
		}

		// emit results every second
		quint64 NewStamp = GetTime() * 1000;
		if (TimeStamp + 1000 < NewStamp)
		{
			TimeStamp = NewStamp;

			emit Results(List);
			List.clear();
		}
	}

	emit Results(List);

	emit Finished();
}