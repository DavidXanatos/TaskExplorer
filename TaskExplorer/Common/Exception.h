#pragma once
#include <stdarg.h>

//
// Qt-free on purpose: this header is included by the variant serialisation that
// TaskHelper shares with the GUI, and the helper links no Qt.
//
#include "../../MiscHelpers/Common/Compat.h"

class CException
{
public:
	CException(const wchar_t *sLine, ...)
	{
		va_list argptr; va_start(argptr, sLine); StrFormat(sLine, argptr); va_end(argptr);
	}

	const std::wstring&		GetLine() const {return m_sLine;}

protected:
	void StrFormat(const wchar_t *sLine, va_list argptr)
	{

		const size_t bufferSize = 10241;
		wchar_t bufferline[bufferSize];
#ifndef WIN32
		if (vswprintf_l(bufferline, bufferSize, sLine, argptr) == -1)
#else
		if (vswprintf(bufferline, bufferSize, sLine, argptr) == -1)
#endif
			bufferline[bufferSize - 1] = L'\0';

		m_sLine = bufferline;
	}

	std::wstring m_sLine;
};