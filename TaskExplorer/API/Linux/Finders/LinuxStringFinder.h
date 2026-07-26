#pragma once
#include "../../Finders/AbstractFinder.h"
#include "../../../../MiscHelpers/Common/FlexError.h"

//
// Scans process memory for printable strings matching RegExp.
//
// Regions are selected per SMemOptions (private / image / mapped) from
// /proc/<pid>/maps and read through CLinuxMemIO.
//
class CLinuxStringFinder : public CAbstractFinder
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxStringFinder)
public:
	CLinuxStringFinder(const SMemOptions& Options, const QRegularExpression& RegExp,
	                   const CProcessPtr& pProcess = CProcessPtr(), QObject* parent = nullptr);
	virtual ~CLinuxStringFinder();

protected:
	virtual void			run();

	SMemOptions				m_Options;
	QRegularExpression		m_RegExp;
	CProcessPtr				m_pProcess;

private:
	STATUS					FindStrings(const CProcessPtr& pProcess);
};
