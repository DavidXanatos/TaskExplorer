#pragma once
#include "../../AssemblyList.h"

class CAssemblyEnum : public QThread
{
	Q_OBJECT

public:
	CAssemblyEnum(quint64 ProcessId, QObject *parent = nullptr);
	virtual ~CAssemblyEnum();


signals:
	void				Assemblies(const CAssemblyListPtr& List);
	void				Finished();

protected:
	void				run();

	//bool				m_bCancel;
	quint64				m_ProcessId;

private:
	static void			AddNodes(CAssemblyListPtr& List, struct _PH_LIST* NodeList, quint64 ParrentId = 0);
};