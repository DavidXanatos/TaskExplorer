/*
 * Task Explorer -
 *   qt wrapper and support functions based on memsrch.c
 *
 * Copyright (C) 2010 wj32
 * Copyright (C) 2017 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "WinStringFinder.h"
#include "../ProcessHacker.h"
#include "../../StringInfo.h"
#include "../WindowsAPI.h"

CWinStringFinder::CWinStringFinder(const SMemOptions& Options, const QRegExp& RegExp, const CProcessPtr& pProcess, QObject* parent) : CAbstractFinder(parent) 
{
	m_Options = Options;
	m_RegExp = RegExp;
	m_pProcess = pProcess;
}

CWinStringFinder::~CWinStringFinder() 
{
}

void CWinStringFinder::run()
{
	if (!m_pProcess.isNull())
	{
		STATUS status = FindStrings(m_pProcess);
		if (status.IsError())
			emit Error(status.GetText(), status.GetStatus());
	}
	else
	{
		QMap<quint64, CProcessPtr> Processes = theAPI->GetProcessList();
		int Modulo = Processes.count() / 100;
		int i = 0;
		for (QMap<quint64, CProcessPtr>::iterator I = Processes.begin(); I != Processes.end() && !m_bCancel; ++I)
		{
			if (Modulo && (i++ % Modulo) == 0)
				emit Progress(float(i) / Processes.count(), I.value()->GetName());

			STATUS status = FindStrings(I.value());
			if (status.IsError() && status.GetStatus() > 0)
			{
				emit Error(status.GetText(), status.GetStatus());
				break;
			}
		}
	}

	emit Finished();
}

#define PH_DISPLAY_BUFFER_COUNT (PAGE_SIZE * 2 - 1)

STATUS CWinStringFinder::FindStrings(const CProcessPtr& pProcess)
{
	if (pProcess->GetProcessId() == (quint64)NtCurrentProcessId())
		return OK;

	QList<QSharedPointer<QObject>> List;

	NTSTATUS status;
    HANDLE processHandle;
	ULONG minimumLength;
    BOOLEAN detectUnicode;
    BOOLEAN extendedUnicode;
    ULONG memoryTypeMask;
    PVOID baseAddress;
    MEMORY_BASIC_INFORMATION basicInfo;
    PUCHAR buffer;
    SIZE_T bufferSize;
    PWSTR displayBuffer;
    SIZE_T displayBufferCount;

    minimumLength = m_Options.MinLength;
	memoryTypeMask = 0;
    if (m_Options.Private)
        memoryTypeMask |= MEM_PRIVATE;
    if (m_Options.Image)
        memoryTypeMask |= MEM_IMAGE;
    if (m_Options.Mapped)
        memoryTypeMask |= MEM_MAPPED;
    detectUnicode = m_Options.Unicode;
    extendedUnicode = m_Options.ExtUnicode;

	QByteArray hex;
	if (minimumLength == -1)
	{
		hex = QByteArray::fromHex(m_RegExp.pattern().toLatin1());
		if (hex.length() < 2)
			return ERR(tr("Match String to short, min length 2"), ERROR_PARAMS);
	}
	else if (minimumLength < 4)
        return ERR(tr("Match String to short, min length 4"), ERROR_PARAMS);

    baseAddress = (PVOID)0;

    bufferSize = PAGE_SIZE * 64;
    buffer = (PUCHAR)PhAllocatePage(bufferSize, NULL);

    if (!buffer)
        return ERR(tr("Allocation error"), ERROR_INTERNAL);

    displayBufferCount = PH_DISPLAY_BUFFER_COUNT;
    displayBuffer = (PWSTR)PhAllocatePage((displayBufferCount + 1) * sizeof(WCHAR), NULL);

    if (!displayBuffer)
    {
        PhFreePage(buffer);
        return ERR(tr("Allocation error"), ERROR_INTERNAL);
    }

    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, (HANDLE)pProcess->GetProcessId())))
    {
		PhFreePage(buffer);
		PhFreePage(displayBuffer);
        return ERR(tr("Unable to open the process"), status);
    }

    while (NT_SUCCESS(NtQueryVirtualMemory(processHandle, baseAddress, MemoryBasicInformation, &basicInfo, sizeof(MEMORY_BASIC_INFORMATION), NULL)))
    {
        ULONG_PTR offset;
        SIZE_T readSize;

        if (m_bCancel)
            break;

        if (basicInfo.State != MEM_COMMIT)
            goto ContinueLoop;
        if ((basicInfo.Type & memoryTypeMask) == 0)
            goto ContinueLoop;
        if (basicInfo.Protect == PAGE_NOACCESS)
            goto ContinueLoop;
        if (basicInfo.Protect & PAGE_GUARD)
            goto ContinueLoop;

        readSize = basicInfo.RegionSize;

        if (basicInfo.RegionSize > bufferSize)
        {
            // Don't allocate a huge buffer though.
            if (basicInfo.RegionSize <= 256 * 1024 * 1024) // 256 MB
            {
                PhFreePage(buffer);
                bufferSize = basicInfo.RegionSize;
                buffer = (PUCHAR)PhAllocatePage(bufferSize, NULL);

                if (!buffer)
                    break;
            }
            else
            {
                readSize = bufferSize;
            }
        }

        for (offset = 0; offset < basicInfo.RegionSize; offset += readSize)
        {
            ULONG_PTR i;
            UCHAR byte; // current byte
            UCHAR byte1; // previous byte
            UCHAR byte2; // byte before previous byte
            BOOLEAN printable;
            BOOLEAN printable1;
            BOOLEAN printable2;
            ULONG length;

			if (m_bCancel)
				break;

            if (!NT_SUCCESS(NtReadVirtualMemory(processHandle, PTR_ADD_OFFSET(baseAddress, offset), buffer, readSize, NULL )))
                continue;

			if (minimumLength == -1) // raw hex search
			{
				QByteArray temp = QByteArray::fromRawData((char*)buffer, readSize); // Constructs a QByteArray using the provided buffer, no copying of data.
				ULONG lengthInBytes = hex.length();
				for (i = 0; i < readSize; )
				{
					int pos = temp.indexOf(hex, i);
					if (pos == -1)
						break;

					QString DisplayStr = QString::fromLatin1(temp.mid(pos, qMin(128, (int)(readSize - pos))));
					CStringInfo* pString = new CStringInfo((quint64)(PTR_ADD_OFFSET(baseAddress, pos)), lengthInBytes, (quint64)basicInfo.BaseAddress, (quint64)basicInfo.RegionSize, DisplayStr, pProcess);
					List.append(CStringInfoPtr(pString));

					i += pos + lengthInBytes;
				}
			}
			else // string search
			{

				byte1 = 0;
				byte2 = 0;
				printable1 = FALSE;
				printable2 = FALSE;
				length = 0;

				for (i = 0; i < readSize; i++)
				{
					byte = buffer[i];

					// dmex: We don't want to enable extra bits in the PhCharIsPrintable array by default
					// or we'll get higher amounts of false positive search results. If the user selects the 
					// ExtendedUnicode option then we'll use iswprint (GetStringTypeW) which does check 
					// every available character by default.
					if (detectUnicode && extendedUnicode && !iswascii(byte))
						printable = !!iswprint(byte);
					else
						printable = PhCharIsPrintable[byte];

					// To find strings Process Hacker uses a state table.
					// * byte2 - byte before previous byte
					// * byte1 - previous byte
					// * byte - current byte
					// * length - length of current string run
					//
					// The states are described below.
					//
					//    [byte2] [byte1] [byte] ...
					//    [char] means printable, [oth] means non-printable.
					//
					// 1. [char] [char] [char] ...
					//      (we're in a non-wide sequence)
					//      -> append char.
					// 2. [char] [char] [oth] ...
					//      (we reached the end of a non-wide sequence, or we need to start a wide sequence)
					//      -> if current string is big enough, create result (non-wide).
					//         otherwise if byte = null, reset to new string with byte1 as first character.
					//         otherwise if byte != null, reset to new string.
					// 3. [char] [oth] [char] ...
					//      (we're in a wide sequence)
					//      -> (byte1 should = null) append char.
					// 4. [char] [oth] [oth] ...
					//      (we reached the end of a wide sequence)
					//      -> (byte1 should = null) if the current string is big enough, create result (wide).
					//         otherwise, reset to new string.
					// 5. [oth] [char] [char] ...
					//      (we reached the end of a wide sequence, or we need to start a non-wide sequence)
					//      -> (excluding byte1) if the current string is big enough, create result (wide).
					//         otherwise, reset to new string with byte1 as first character and byte as
					//         second character.
					// 6. [oth] [char] [oth] ...
					//      (we're in a wide sequence)
					//      -> (byte2 and byte should = null) do nothing.
					// 7. [oth] [oth] [char] ...
					//      (we're starting a sequence, but we don't know if it's a wide or non-wide sequence)
					//      -> append char.
					// 8. [oth] [oth] [oth] ...
					//      (nothing)
					//      -> do nothing.

					if (printable2 && printable1 && printable)
					{
						if (length < displayBufferCount)
							displayBuffer[length] = byte;

						length++;
					}
					else if (printable2 && printable1 && !printable)
					{
						if (length >= minimumLength)
						{
							goto CreateResult;
						}
						else if (byte == 0)
						{
							length = 1;
							displayBuffer[0] = byte1;
						}
						else
						{
							length = 0;
						}
					}
					else if (printable2 && !printable1 && printable)
					{
						if (byte1 == 0)
						{
							if (length < displayBufferCount)
								displayBuffer[length] = byte;

							length++;
						}
					}
					else if (printable2 && !printable1 && !printable)
					{
						if (length >= minimumLength)
						{
							goto CreateResult;
						}
						else
						{
							length = 0;
						}
					}
					else if (!printable2 && printable1 && printable)
					{
						if (length >= minimumLength + 1) // length - 1 >= minimumLength but avoiding underflow
						{
							length--; // exclude byte1
							goto CreateResult;
						}
						else
						{
							length = 2;
							displayBuffer[0] = byte1;
							displayBuffer[1] = byte;
						}
					}
					else if (!printable2 && printable1 && !printable)
					{
						// Nothing
					}
					else if (!printable2 && !printable1 && printable)
					{
						if (length < displayBufferCount)
							displayBuffer[length] = byte;

						length++;
					}
					else if (!printable2 && !printable1 && !printable)
					{
						// Nothing
					}

					goto AfterCreateResult;

CreateResult:
					{
						ULONG lengthInBytes;
						ULONG bias;
						BOOLEAN isWide;
						ULONG displayLength;

						lengthInBytes = length;
						bias = 0;
						isWide = FALSE;

						if (printable1 == printable) // determine if string was wide (refer to state table, 4 and 5)
						{
							isWide = TRUE;
							lengthInBytes *= 2;
						}

						if (printable) // byte1 excluded (refer to state table, 5)
						{
							bias = 1;
						}

						if (!(isWide && !detectUnicode))
						{
							//result = PhCreateMemoryResult(PTR_ADD_OFFSET(baseAddress, i - bias - lengthInBytes), baseAddress, lengthInBytes))
							displayLength = (ULONG)(min(length, displayBufferCount) * sizeof(WCHAR));
							wstring displayStr(displayLength / sizeof(WCHAR) + 1, L'0');
							memcpy((void*)displayStr.c_str(), displayBuffer, displayLength);

							QString DisplayStr = QString::fromStdWString(displayStr);
							if (DisplayStr.contains(m_RegExp))
							{
								CStringInfo* pString = new CStringInfo((quint64)(PTR_ADD_OFFSET(baseAddress, i - bias - lengthInBytes)), lengthInBytes, (quint64)basicInfo.BaseAddress, (quint64)basicInfo.RegionSize, DisplayStr, pProcess);
								List.append(CStringInfoPtr(pString));
							}
						}

						length = 0;
					}
AfterCreateResult:

					byte2 = byte1;
					byte1 = byte;
					printable2 = printable1;
					printable1 = printable;
				}
			}
        }

ContinueLoop:
        baseAddress = PTR_ADD_OFFSET(baseAddress, basicInfo.RegionSize);
    }

    if (buffer)
        PhFreePage(buffer);

	if (displayBuffer)
		PhFreePage(displayBuffer);

	emit Results(List);

	if(!buffer)
		return ERR(tr("Allocation error"), ERROR_INTERNAL);

	return OK;
}