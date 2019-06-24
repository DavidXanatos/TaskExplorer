#pragma once

#define ERROR_UNDEFINED (-1)
#define ERROR_CONFIRM (-2)

class CFlexError
{
public:
	CFlexError() 
	{
		m = NULL;
	}
	CFlexError(const QString& Error, ulong Status = ERROR_UNDEFINED) : CFlexError() 
	{
		Attach(MkError(Error, Status));
	}
	CFlexError(ulong Status) : CFlexError(QObject::tr("Error"), Status) 
	{
	}
	CFlexError(const CFlexError& other) : CFlexError() 
	{
		if(other.m != NULL)
			Attach((SFlexError*)other.m);
	}
	virtual ~CFlexError() 
	{
		Detach();
	}

	CFlexError& operator=(const CFlexError& Array) 
	{
		Attach(Array.m); 
		return *this; 
	}

	__inline bool IsError() const { return m != NULL; }
	__inline ulong GetStatus() const { return m ? m->Status : 0; }
	__inline QString GetText() const { return m ? m->Error: ""; }

	operator bool() const				{return !IsError();}

private:
	struct SFlexError
	{
		QString Error;
		ulong Status;

		mutable atomic<int> aRefCnt;
	} *m;

	SFlexError* MkError(const QString& Error, ulong Status)
	{
		SFlexError* p = new SFlexError();
		p->Error = Error;
		p->Status = Status;
		return p;
	}

	void Attach(SFlexError* p)
	{
		Detach();

		if (p != NULL)
		{
			p->aRefCnt.fetch_add(1);
			m = p;
		}
	}

	void Detach()
	{
		if (m != NULL)
		{
			if (m->aRefCnt.fetch_sub(1) == 1)
				delete m;
			m = NULL;
		}
	}
};

typedef CFlexError STATUS;
#define OK CFlexError()
#define ERR CFlexError