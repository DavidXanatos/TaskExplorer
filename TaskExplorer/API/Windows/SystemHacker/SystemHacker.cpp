/*
* This file is part of TaskExplorer.
*
* MIT License
* 
* Copyright (C) 2025 DavidXanatos
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "stdafx.h"
#include "SystemHacker.h"
#include "xphuser.h"
extern "C" {
#include "xphuserp.h"
}

STATUS InitKSH(QString DeviceName, QString FileName, int SecurityLevel)
{
	if (DeviceName.isEmpty())
		DeviceName = QString::fromWCharArray(KPH_DEVICE_SHORT_NAME);
	if (FileName.isEmpty())
		FileName = "KSystemHacker.sys";

	// if the file name is not a full path Add the application directory
	if (!FileName.contains("\\"))
		FileName = QApplication::applicationDirPath() + "/" + FileName;

	FileName = FileName.replace("/", "\\");
    if (!QFile::exists(FileName))
		return ERR(QObject::tr("The Process Hacker kernel driver '%1' was not found.").arg(FileName), STATUS_NOT_FOUND);

	KPH_PARAMETERS parameters;
    parameters.SecurityLevel = (KPH_SECURITY_LEVEL)SecurityLevel;
    parameters.CreateDynamicConfiguration = TRUE;
	NTSTATUS status = XphConnect2Ex((wchar_t*)DeviceName.toStdWString().c_str(), (wchar_t*)FileName.toStdWString().c_str(), &parameters);
    if (!NT_SUCCESS(status))
		return ERR(QObject::tr("Unable to load the kernel driver, Error: 0x%1").arg((quint32)status, 8, 16, QChar('0')), status);
    
	PUCHAR signature = NULL;
    ULONG signatureSize = 0;

	//QString SigFileName = QApplication::applicationDirPath() + "/TaskExplorer.sig";
	//SigFileName = SigFileName.replace("/", "\\");
	//if (QFile::exists(SigFileName))
	//	PhpReadSignature((wchar_t*)SigFileName.toStdWString().c_str(), &signature, &signatureSize);

	// Note: Debug driver accepts a empty signature as valid
	if (NT_SUCCESS(XphVerifyClient(signature, signatureSize)))
	{
#ifdef _DEBUG
		KPH_KEY Key;
		KPHP_GET_L1_KEY_CONTEXT Context = { &Key };
		XphpGetL1KeyContinuation(-1, &Context); // "Master"-Key LOL
#endif
		qDebug() << "Successfully verified the client signature.";
	}
	else
		qDebug() << "Unable to verify the client signature.";

	if(signature)
		free(signature);

	return OK;
}

BOOLEAN NTAPI KshIsConnected(VOID)
{
	return XphIsConnected();
}

NTSTATUS NTAPI KshGetFeatures(_Inout_ PULONG Features)
{
	return XphGetFeatures(Features);
}