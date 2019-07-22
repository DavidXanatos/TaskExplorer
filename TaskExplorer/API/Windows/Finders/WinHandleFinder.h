#pragma once
#include "../../Finders/AbstractFinder.h"

class CWinHandleFinder : public CAbstractFinder
{
	Q_OBJECT

public:
	CWinHandleFinder(const QVariant& Type, const QRegExp& RegExp, QObject* parent = NULL);
	virtual ~CWinHandleFinder();

protected:
	virtual void run();

	int m_Type;
	QRegExp m_RegExp;
};