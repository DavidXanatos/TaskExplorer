#pragma once
#include "../../Finders/AbstractFinder.h"

class CWinModuleFinder : public CAbstractFinder
{
	Q_OBJECT

public:
	CWinModuleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent = NULL);
	virtual ~CWinModuleFinder();

protected:
	virtual void run();

	QVariant m_Type;
	QRegularExpression m_RegExp;
};