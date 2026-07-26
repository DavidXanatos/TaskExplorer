#pragma once
#include "../../Finders/AbstractFinder.h"

//
// Searches every process's mapped shared objects for a path matching RegExp,
// by scanning the file-backed entries of each /proc/<pid>/maps.
//
class CLinuxModuleFinder : public CAbstractFinder
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxModuleFinder)
public:
	CLinuxModuleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent = nullptr);
	virtual ~CLinuxModuleFinder();

protected:
	virtual void			run();

	QVariant				m_Type;
	QRegularExpression		m_RegExp;
};
