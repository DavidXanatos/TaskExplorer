#pragma once

#include "../ProcessInfo.h"

class CAbstractFinder : public QThread
{
	Q_OBJECT

public:
	CAbstractFinder(QObject* parent);
	virtual ~CAbstractFinder();

	virtual void Cancel() { m_bCancel = true; }
	virtual bool IsCanceled() { return m_bCancel; }

	static CAbstractFinder* FindHandles(const QVariant& Type, const QRegExp& RegExp);
	static CAbstractFinder* FindModules(const QVariant& Type, const QRegExp& RegExp);

	struct SMemOptions
	{
		int MinLength;
		bool Unicode;
		bool ExtUnicode;
		// regions
		bool Private;
		bool Image;
		bool Mapped;
	};
	static CAbstractFinder* FindStrings(const SMemOptions& Options, const QRegExp& RegExp, const CProcessPtr& pProcess = CProcessPtr());

signals:
	void	Progress(float value, const QString& Info = QString());
	void	Results(QList<QSharedPointer<QObject>> List);
	void	Error(const QString& Error, int Code);
	void	Finished();

protected:
	bool	m_bCancel;
};