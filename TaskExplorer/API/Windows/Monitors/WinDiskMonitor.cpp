/*
 * Process Hacker Plugins -
 *   qt port of Hardware Devices Plugin
 *
 * Copyright (C) 2015-2018 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "WinDiskMonitor.h"
#include "../ProcessHacker.h"
#include "../ProcessHacker/devices.h"
#include <cfgmgr32.h>
#include <devguid.h>

CWinDiskMonitor::CWinDiskMonitor(QObject *parent)
	: CDiskMonitor(parent)
{
}

CWinDiskMonitor::~CWinDiskMonitor()
{
	//foreach(SDiskInfo* entry, m_DiskList)
	//	delete diskEntry;
	m_DiskList.clear();
}

bool CWinDiskMonitor::Init()
{
	if (!UpdateDisks())
		return false;

	return true;
}

BOOLEAN QueryDiskDeviceInterfaceDescription(
    _In_ PWSTR DeviceInterface,
    _Out_ DEVINST *DeviceInstanceHandle,
    _Out_ PPH_STRING *DeviceDescription
    )
{
    CONFIGRET result;
    ULONG bufferSize;
    PPH_STRING deviceDescription;
    DEVPROPTYPE devicePropertyType;
    DEVINST deviceInstanceHandle;
    ULONG deviceInstanceIdLength = MAX_DEVICE_ID_LEN;
    WCHAR deviceInstanceId[MAX_DEVICE_ID_LEN + 1] = L"";

    if (CM_Get_Device_Interface_Property(
        DeviceInterface,
        &DEVPKEY_Device_InstanceId,
        &devicePropertyType,
        (PBYTE)deviceInstanceId,
        &deviceInstanceIdLength,
        0
        ) != CR_SUCCESS)
    {
        return FALSE;
    }

    if (CM_Locate_DevNode(
        &deviceInstanceHandle,
        deviceInstanceId,
        CM_LOCATE_DEVNODE_PHANTOM
        ) != CR_SUCCESS)
    {
        return FALSE;
    }

    bufferSize = 0x40;
    deviceDescription = PhCreateStringEx(NULL, bufferSize);

    if ((result = CM_Get_DevNode_Property(
        deviceInstanceHandle,
        &DEVPKEY_Device_FriendlyName,
        &devicePropertyType,
        (PBYTE)deviceDescription->Buffer,
        &bufferSize,
        0
        )) != CR_SUCCESS)
    {
        PhDereferenceObject(deviceDescription);
        deviceDescription = PhCreateStringEx(NULL, bufferSize);

        result = CM_Get_DevNode_Property(
            deviceInstanceHandle,
            &DEVPKEY_Device_FriendlyName,
            &devicePropertyType,
            (PBYTE)deviceDescription->Buffer,
            &bufferSize,
            0
            );
    }

    if (result != CR_SUCCESS)
    {
        PhDereferenceObject(deviceDescription);
        return FALSE;
    }

    PhTrimToNullTerminatorString(deviceDescription);

    *DeviceInstanceHandle = deviceInstanceHandle;
    *DeviceDescription = deviceDescription;

    return TRUE;
}

bool CWinDiskMonitor::UpdateDisks()
{
    PWSTR deviceInterfaceList;
    ULONG deviceInterfaceListLength = 0;
    PWSTR deviceInterface;

    if (CM_Get_Device_Interface_List_Size(
        &deviceInterfaceListLength,
        (PGUID)&GUID_DEVINTERFACE_DISK,
        NULL,
        CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES
        ) != CR_SUCCESS)
    {
        return false;
    }

    deviceInterfaceList = (PWSTR)PhAllocate(deviceInterfaceListLength * sizeof(WCHAR));
    memset(deviceInterfaceList, 0, deviceInterfaceListLength * sizeof(WCHAR));

    if (CM_Get_Device_Interface_List(
        (PGUID)&GUID_DEVINTERFACE_DISK,
        NULL,
        deviceInterfaceList,
        deviceInterfaceListLength,
        CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES
        ) != CR_SUCCESS)
    {
        PhFree(deviceInterfaceList);
        return false;
    }

	QWriteLocker Locker(&m_StatsMutex);

	PH_AUTO_POOL autoPool;
	PhInitializeAutoPool(&autoPool);

	QSet<QString> OldDisks = m_DiskList.keys().toSet();

	for (deviceInterface = deviceInterfaceList; *deviceInterface; deviceInterface += PhCountStringZ(deviceInterface) + 1)
	{
		DEVINST deviceInstanceHandle;
		PPH_STRING deviceDescription = NULL;
		HANDLE deviceHandle;

		if (!QueryDiskDeviceInterfaceDescription(deviceInterface, &deviceInstanceHandle, &deviceDescription))
			continue;

		QString DevicePath = QString::fromWCharArray(deviceInterface);
		
		SDiskInfo* diskEntry;
		QMap<QString, SDiskInfo>::iterator I = m_DiskList.find(DevicePath);
		if (I == m_DiskList.end())
		{
			I = m_DiskList.insert(DevicePath, SDiskInfo());
			diskEntry = &I.value();
			diskEntry->DevicePath = DevicePath;
			diskEntry->DeviceName = QString::fromWCharArray(deviceDescription->sr.Buffer, deviceDescription->sr.Length / sizeof(wchar_t));
		}
		else
		{
			OldDisks.remove(DevicePath);
			diskEntry = &I.value();
		}

		if (NT_SUCCESS(PhCreateFileWin32(
			&deviceHandle,
			deviceInterface,
			FILE_READ_ATTRIBUTES | SYNCHRONIZE,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
		)))
		{
			ULONG diskIndex = ULONG_MAX; // Note: Do not initialize to zero

			if (NT_SUCCESS(DiskDriveQueryDeviceTypeAndNumber(
				deviceHandle,
				&diskIndex,
				NULL
			)))
			{
				PPH_STRING diskMountPoints;

				diskMountPoints = PH_AUTO_T(PH_STRING, DiskDriveQueryDosMountPoints(diskIndex));

				diskEntry->DeviceIndex = diskIndex;
				diskEntry->DevicePresent = true;
				diskEntry->DeviceSupported = true;

				if (!PhIsNullOrEmptyString(diskMountPoints))
				{
					//diskEntry->DeviceMountPoints = QString::fromWCharArray(diskMountPoints->Buffer, diskMountPoints->Length / sizeof(wchar_t));
					diskEntry->DeviceMountPoints = tr("Disk %1 (%2) [%3]").arg(diskIndex)
						.arg(QString::fromWCharArray(diskMountPoints->Buffer, diskMountPoints->Length / sizeof(wchar_t)))
						.arg(QString::fromWCharArray(deviceDescription->Buffer, deviceDescription->Length / sizeof(wchar_t)));
				}
				else
				{
					//diskEntry->DeviceMountPoints = tr("Disk %1").arg(diskIndex);
					diskEntry->DeviceMountPoints = tr("Disk %1 [%2]").arg(diskIndex)
						.arg(QString::fromWCharArray(deviceDescription->Buffer, deviceDescription->Length / sizeof(wchar_t)));
				}
			}
			else
				diskEntry->DevicePresent = false;

			NtClose(deviceHandle);
		}
		else
			diskEntry->DevicePresent = false;

		PhDereferenceObject(deviceDescription);
	}

	foreach(const QString& DevicePath, OldDisks)
		m_DiskList.remove(DevicePath);

	PhDeleteAutoPool(&autoPool);

	return true;
}

void CWinDiskMonitor::UpdateDiskStats()
{
	quint64 curTick = GetCurTick();

	QWriteLocker Locker(&m_StatsMutex);

	for(QMap<QString, SDiskInfo>::iterator I = m_DiskList.begin(); I != m_DiskList.end(); I++)
	{
		SDiskInfo* entry = &I.value();
		//if (!entry->DevicePresent)
		//	continue;

		wstring DeviceInterface = entry->DevicePath.toStdWString();
		PWSTR deviceInterface = (PWSTR)DeviceInterface.c_str();
		HANDLE deviceHandle;
		if (NT_SUCCESS(PhCreateFileWin32(
			&deviceHandle,
			deviceInterface,
			FILE_READ_ATTRIBUTES | SYNCHRONIZE,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
		)))
		{
			DISK_PERFORMANCE diskPerformance;

			if (NT_SUCCESS(DiskDriveQueryStatistics(deviceHandle, &diskPerformance)))
			{
				ULONG64 readTime;
                ULONG64 writeTime;
                ULONG64 idleTime;
                ULONG readCount;
                ULONG writeCount;
                ULONG64 queryTime;

				entry->SetRead(diskPerformance.BytesRead.QuadPart, diskPerformance.ReadCount);
				entry->SetWrite(diskPerformance.BytesWritten.QuadPart, diskPerformance.WriteCount);
                //entry->BytesReadDelta.Update(diskPerformance.BytesRead.QuadPart);
                //entry->BytesWrittenDelta.Update(diskPerformance.BytesWritten.QuadPart);
                entry->ReadTimeDelta.Update(diskPerformance.ReadTime.QuadPart);
                entry->WriteTimeDelta.Update(diskPerformance.WriteTime.QuadPart);
                entry->IdleTimeDelta.Update(diskPerformance.IdleTime.QuadPart);
                //entry->ReadCountDelta.Update(diskPerformance.ReadCount);
                //entry->WriteCountDelta.Update(diskPerformance.WriteCount);
                entry->QueryTimeDelta.Update(diskPerformance.QueryTime.QuadPart);

				quint64 time_ms = curTick - entry->LastStatUpdate;
				entry->LastStatUpdate = curTick;
				entry->UpdateStats(time_ms);

                readTime = entry->ReadTimeDelta.Delta;
                writeTime = entry->WriteTimeDelta.Delta;
                idleTime = entry->IdleTimeDelta.Delta;
				readCount = entry->ReadDelta.Delta; //entry->ReadCountDelta.Delta;
				writeCount = entry->WriteDelta.Delta; //entry->WriteCountDelta.Delta;
                queryTime = entry->QueryTimeDelta.Delta;

                if (readCount + writeCount != 0)
                    entry->ResponseTime = (((FLOAT)readTime + (FLOAT)writeTime) / (readCount + writeCount)) / PH_TICKS_PER_MS;
                else
                    entry->ResponseTime = 0;

                if (queryTime != 0)
                    entry->ActiveTime = (FLOAT)(queryTime - idleTime) / queryTime * 100;
                else
                    entry->ActiveTime = 0.0f;

                if (entry->ActiveTime > 100.f)
                    entry->ActiveTime = 0.f;
                if (entry->ActiveTime < 0.f)
                    entry->ActiveTime = 0.f;

                entry->QueueDepth = diskPerformance.QueueDepth;
                entry->SplitCount = diskPerformance.SplitCount;
                entry->DiskIndex = diskPerformance.StorageDeviceNumber;

				entry->DevicePresent = true;
			}
			else
			{
				// not supported on this disk, for example a intel eRST raid 5 drive (at least with the drivers I head) fails here.
				entry->DeviceSupported = false;
			}

			// Note: we need to re query this for removable media
			entry->TotalSize = DiskDriveQuerySize(deviceHandle);

			NtClose(deviceHandle);
		}
		else
		{
			entry->DevicePresent = false;
		}
	}
}
