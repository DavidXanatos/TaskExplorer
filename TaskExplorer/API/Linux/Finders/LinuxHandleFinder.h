#pragma once
#include "../../Finders/AbstractFinder.h"

//
// Searches every process's open file descriptors for a name matching RegExp.
// Walks /proc/<pid>/fd across all pids; entries that cannot be read (other
// users, when unprivileged) are skipped rather than reported as errors.
//
class CLinuxHandleFinder : public CAbstractFinder
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxHandleFinder)
public:
	CLinuxHandleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent = nullptr);
	virtual ~CLinuxHandleFinder();

protected:
	virtual void			run();

	QVariant				m_Type;
	QRegularExpression		m_RegExp;
};
