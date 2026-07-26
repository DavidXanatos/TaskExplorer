//
// No stdafx.h and no Qt on purpose: this translation unit is compiled into both
// TaskExplorer and the Qt-free TaskHelper.
//

#include "Compat.h"

#ifndef WIN32

#include <string.h>

int vswprintf_l(wchar_t* _String, size_t _Count, const wchar_t* _Format, va_list _Ap)
{
	//
	// Swap %s and %S, which MSVC and glibc interpret oppositely in a wide format
	// string. Done on a copy so the caller's literal is untouched.
	//
	// The bound is checked rather than asserted: this runs in the helper too,
	// where an over-long format string must degrade rather than trip an assert
	// that is compiled out anyway.
	//
	const size_t MaxFormat = 1024;

	wchar_t Format[MaxFormat + 1];
	const size_t Length = wcslen(_Format);
	if (Length > MaxFormat)
		return vswprintf(_String, _Count, _Format, _Ap);	// too long to rewrite; pass through

	wcscpy(Format, _Format);

	for (size_t i = 0; i < Length; i++)
	{
		if (Format[i] != L'%')
			continue;

		switch (Format[i + 1])
		{
			case L's':	Format[i + 1] = L'S'; break;
			case L'S':	Format[i + 1] = L's'; break;
			// "%%" is an escaped percent, so skip its second character to avoid
			// misreading the next one as a specifier.
			case L'%':	i++; break;
		}
	}

	return vswprintf(_String, _Count, Format, _Ap);
}

#endif // !WIN32
