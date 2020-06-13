#pragma once
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"

class CProcessFilterModel : public CSortFilterProxyModel
{
public:
	CProcessFilterModel(bool bAlternate, QObject* parrent = 0);

	bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const;

	virtual void SetEnabled(bool bSet)			{m_bEnabled = bSet; invalidate(); }
	virtual bool IsEnabled() const 				{return m_bEnabled;}
	virtual void SetFilterSystem(int iSet)		{m_iFilterSystem = iSet; invalidate(); }
#ifdef WIN32
	virtual void SetFilterWindows(int iSet)		{m_iFilterWindows = iSet; invalidate(); }
#endif
	virtual void SetFilterService(int iSet)		{m_iFilterService = iSet; invalidate(); }
	virtual void SetFilterOther(int iSet)		{m_iFilterOther = iSet; invalidate(); }
	virtual void SetFilterOwn(int iSet)			{m_iFilterOwn = iSet; invalidate(); }

	virtual void UpdateUsers(const QSet<QString>& Users) {m_Users = Users; invalidate(); }

protected:
	bool m_bEnabled;
	int m_iFilterSystem;
#ifdef WIN32
	int m_iFilterWindows;
#endif
	int m_iFilterService;
	int m_iFilterOther;
	int m_iFilterOwn;

	QSet<QString> m_Users;
};