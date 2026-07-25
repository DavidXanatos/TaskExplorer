#pragma once
#include "../../Finders/AbstractFinder.h"

class CWinHandleFinder : public CAbstractFinder
{
	Q_OBJECT

	TRACK_OBJECT(CWinHandleFinder)
public:
	CWinHandleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent = NULL);
	virtual ~CWinHandleFinder();

protected:
	virtual void run();

	int m_Type;
	QRegularExpression m_RegExp;
};