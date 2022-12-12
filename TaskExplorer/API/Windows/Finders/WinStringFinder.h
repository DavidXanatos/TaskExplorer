#pragma once
#include "../../Finders/AbstractFinder.h"
#include "../../../../MiscHelpers/Common/FlexError.h"

class CWinStringFinder : public CAbstractFinder
{
	Q_OBJECT

public:
	CWinStringFinder(const SMemOptions& Options, const QRegularExpression& RegExp, const CProcessPtr& pProcess = CProcessPtr(), QObject* parent = NULL);
	virtual ~CWinStringFinder();

protected:
	virtual void run();

	SMemOptions m_Options;
	QRegularExpression m_RegExp;
	CProcessPtr m_pProcess;

private:
	STATUS FindStrings(const CProcessPtr& pProcess);
};