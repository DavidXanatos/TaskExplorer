/*
 * Process Hacker Extended Tools -
 *   qt port of gpumon.c
 *
 * Copyright (C) 2011-2015 wj32
 * Copyright (C) 2016-2018 dmex
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
#include "GpuMonitor.h"
#include "../ProcessHacker.h"
#include "../../GUI/TaskExplorer.h"

#define D3D11_NO_HELPERS
#include <d3d11.h>
extern "C" {
#include "d3dkmt.h"
}

#include <cfgmgr32.h>
#include <ntddvdeo.h>

#define BYTES_NEEDED_FOR_BITS(Bits) ((((Bits) + sizeof(ULONG) * 8 - 1) / 8) & ~(SIZE_T)(sizeof(ULONG) - 1)) // divide round up

struct SGpuAdapter
{
	SGpuAdapter()
	{
		SegmentCount = 0;
		NodeCount = 0;
		//FirstNodeIndex = 0;

		TimeUsage = 0;
	}

	LUID AdapterLuid;
	ulong SegmentCount;
	ulong NodeCount;
	//ulong FirstNodeIndex;

	CGpuMonitor::SGpuInfo Info;

	RTL_BITMAP ApertureBitMap;
	QVector<ulong> ApertureBitMapBuffer;

	CGpuMonitor::SGpuMemory GpuMemory;

	QVector<SDelta64> TotalTimeDeltas;
	float TimeUsage;
};

CGpuMonitor::CGpuMonitor(QObject *parent)
	: QObject(parent)
{
	//m_GpuTotalNodeCount = 0;
	//m_GpuTotalSegmentCount = 0;
	//m_GpuNextNodeIndex = 0;

	//m_GpuDedicatedLimit = 0;
	//m_GpuDedicatedUsage = 0;
	//m_GpuSharedLimit = 0;
	//m_GpuSharedUsage = 0;

	//m_GpuNodeUsage = 0;

	m_ClockTotalRunningTimeFrequency = 0;
}

CGpuMonitor::~CGpuMonitor()
{
	foreach(SGpuAdapter* adapter, m_GpuAdapterList)
		delete adapter;
	m_GpuAdapterList.clear();
}

bool CGpuMonitor::Init()
{
	if (!InitializeD3DStatistics())
		return false;

	//m_GpuNodesTotalRunningTimeDeltas.resize(m_GpuTotalNodeCount);

	return true;
}

QString CGpuMonitor::QueryDeviceProperty(/*DEVINST*/quint32 DeviceHandle, const /*DEVPROPKEY*/struct _DEVPROPKEY *DeviceProperty)
{
    CONFIGRET result;
    PBYTE buffer;
    ULONG bufferSize;
    DEVPROPTYPE propertyType;

    bufferSize = 0x80;
    buffer = (PBYTE)PhAllocate(bufferSize);
    propertyType = DEVPROP_TYPE_EMPTY;

    if ((result = CM_Get_DevNode_Property(DeviceHandle, DeviceProperty, &propertyType, buffer, &bufferSize, 0)) != CR_SUCCESS)
    {
        PhFree(buffer);
        buffer = (PBYTE)PhAllocate(bufferSize);

        result = CM_Get_DevNode_Property(DeviceHandle, DeviceProperty, &propertyType, buffer, &bufferSize, 0);
    }

    if (result != CR_SUCCESS)
    {
        PhFree(buffer);
        return NULL;
    }

    switch (propertyType)
    {
    case DEVPROP_TYPE_STRING:
        {
            return QString::fromWCharArray((PWCHAR)buffer, bufferSize / sizeof(wchar_t)).trimmed();
        }
        break;
    case DEVPROP_TYPE_FILETIME:
        {
            LARGE_INTEGER FileTime;
			FileTime.HighPart = ((PFILETIME)buffer)->dwHighDateTime;
			FileTime.LowPart = ((PFILETIME)buffer)->dwLowDateTime;
            
			return QDateTime::fromTime_t(FILETIME2time(FileTime.QuadPart)).toString("dd.MM.yyyy hh:mm:ss");
        }
        break;
    case DEVPROP_TYPE_UINT32:
        {
			return QString::number(*(PULONG)buffer);
        }
        break;
    case DEVPROP_TYPE_UINT64:
        {
			return QString::number(*(PULONG64)buffer);
        }
        break;
    }
    PhFree(buffer);

    return NULL;
}

QString CGpuMonitor::QueryDeviceRegistryProperty(/*DEVINST*/quint32 DeviceHandle, ulong DeviceProperty)
{
    CONFIGRET result;
    
    ULONG bufferSize = 0x80;
    PPH_STRING string = PhCreateStringEx(NULL, bufferSize);

	DEVPROPTYPE devicePropertyType;
    if ((result = CM_Get_DevNode_Registry_Property(DeviceHandle, DeviceProperty, &devicePropertyType, (PBYTE)string->Buffer, &bufferSize, 0 )) != CR_SUCCESS)
    {
        PhDereferenceObject(string);

        string = PhCreateStringEx(NULL, bufferSize);

        result = CM_Get_DevNode_Registry_Property(DeviceHandle, DeviceProperty, &devicePropertyType, (PBYTE)string->Buffer, &bufferSize, 0);
    }

	QString Str = CastPhString(string).trimmed();
	if (result != CR_SUCCESS)
		return QString();
	return Str;
}

// Note: MSDN states this value must be created by video devices BUT Task Manager 
// doesn't query this value and I currently don't know where it's getting the gpu memory information.
// https://docs.microsoft.com/en-us/windows-hardware/drivers/display/registering-hardware-information
quint64 CGpuMonitor::QueryGpuInstalledMemory(/*DEVINST*/quint32 DeviceHandle)
{
    ULONG64 installedMemory = ULLONG_MAX;
    HKEY keyHandle;

    if (CM_Open_DevInst_Key(DeviceHandle, KEY_READ, 0, RegDisposition_OpenExisting, &keyHandle, CM_REGISTRY_SOFTWARE) == CR_SUCCESS)
    {
        installedMemory = PhQueryRegistryUlong64(keyHandle, L"HardwareInformation.qwMemorySize");

        if (installedMemory == ULLONG_MAX)
            installedMemory = PhQueryRegistryUlong(keyHandle, L"HardwareInformation.MemorySize");

        if (installedMemory == ULONG_MAX) // HACK
            installedMemory = ULLONG_MAX;

        NtClose(keyHandle);
    }

    return installedMemory;
}

bool CGpuMonitor::QueryDeviceProperties(wchar_t* DeviceInterface, QString* Description, QString* DriverDate, QString* DriverVersion, QString* LocationInfo, quint64* InstalledMemory)
{
    DEVPROPTYPE devicePropertyType;
    DEVINST deviceInstanceHandle;
    ULONG deviceInstanceIdLength = MAX_DEVICE_ID_LEN;
    WCHAR deviceInstanceId[MAX_DEVICE_ID_LEN];

    if (CM_Get_Device_Interface_Property(DeviceInterface, &DEVPKEY_Device_InstanceId, &devicePropertyType, (PBYTE)deviceInstanceId, &deviceInstanceIdLength, 0) != CR_SUCCESS)
        return false;

	if (CM_Locate_DevNode(&deviceInstanceHandle, deviceInstanceId, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
		return false;

    if (Description)
        *Description = QueryDeviceProperty(deviceInstanceHandle, &DEVPKEY_Device_DeviceDesc); // DEVPKEY_Device_Manufacturer
    if (DriverDate)
        *DriverDate = QueryDeviceProperty(deviceInstanceHandle, &DEVPKEY_Device_DriverDate);
    if (DriverVersion)
        *DriverVersion = QueryDeviceProperty(deviceInstanceHandle, &DEVPKEY_Device_DriverVersion);
    if (LocationInfo)
        *LocationInfo = QueryDeviceProperty(deviceInstanceHandle, &DEVPKEY_Device_LocationInfo);
    if (InstalledMemory)
        *InstalledMemory = QueryGpuInstalledMemory(deviceInstanceHandle);
    // EtpQueryDeviceProperty(deviceInstanceHandle, &DEVPKEY_Device_Manufacturer);

    // Undocumented device properties (Win10 only)
    //DEFINE_DEVPROPKEY(DEVPKEY_Gpu_Luid, 0x60b193cb, 0x5276, 0x4d0f, 0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6, 2); // DEVPROP_TYPE_UINT64
    //DEFINE_DEVPROPKEY(DEVPKEY_Gpu_PhysicalAdapterIndex, 0x60b193cb, 0x5276, 0x4d0f, 0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6, 3); // DEVPROP_TYPE_UINT32

    return true;
}

QString CGpuMonitor::GetNodeEngineTypeString(/*D3DKMT_NODEMETADATA**/struct _D3DKMT_NODEMETADATA* NodeMetaData)
{
    switch (NodeMetaData->EngineType)
    {
    case DXGK_ENGINE_TYPE_OTHER:
        return QString::fromWCharArray(NodeMetaData->FriendlyName);
    case DXGK_ENGINE_TYPE_3D:
        return tr("3D");
    case DXGK_ENGINE_TYPE_VIDEO_DECODE:
        return tr("Video Decode");
    case DXGK_ENGINE_TYPE_VIDEO_ENCODE:
        return tr("Video Encode");
    case DXGK_ENGINE_TYPE_VIDEO_PROCESSING:
        return tr("Video Processing");
    case DXGK_ENGINE_TYPE_SCENE_ASSEMBLY:
        return tr("Scene Assembly");
    case DXGK_ENGINE_TYPE_COPY:
        return tr("Copy");
    case DXGK_ENGINE_TYPE_OVERLAY:
        return tr("Overlay");
    case DXGK_ENGINE_TYPE_CRYPTO:
        return tr("Crypto");
    }
    return tr("ERROR (%1)").arg(NodeMetaData->EngineType);
}

D3D_FEATURE_LEVEL EtQueryAdapterFeatureLevel(_In_ LUID AdapterLuid)
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static PFN_D3D11_CREATE_DEVICE D3D11CreateDevice_I = NULL;
    static HRESULT (WINAPI *CreateDXGIFactory1_I)(_In_ REFIID riid, _Out_ PVOID *ppFactory) = NULL;
    D3D_FEATURE_LEVEL d3dFeatureLevelResult = (D3D_FEATURE_LEVEL)0;
    IDXGIFactory1 *dxgiFactory;
    IDXGIAdapter* dxgiAdapter;

    if (PhBeginInitOnce(&initOnce))
    {
        LoadLibrary(L"dxgi.dll");
        LoadLibrary(L"d3d11.dll");
        CreateDXGIFactory1_I = (HRESULT (WINAPI *)(_In_ REFIID riid, _Out_ PVOID *ppFactory))PhGetModuleProcAddress(L"dxgi.dll", "CreateDXGIFactory1");
        D3D11CreateDevice_I = (PFN_D3D11_CREATE_DEVICE)PhGetModuleProcAddress(L"d3d11.dll", "D3D11CreateDevice");

        PhEndInitOnce(&initOnce);
    }

    if (!CreateDXGIFactory1_I || !SUCCEEDED(CreateDXGIFactory1_I(*(_GUID*)&IID_IDXGIFactory1, (PVOID*)&dxgiFactory)))
        return d3dFeatureLevelResult;

    for (UINT i = 0; i < 25; i++)
    {
        DXGI_ADAPTER_DESC dxgiAdapterDescription;

        if (!SUCCEEDED(IDXGIFactory1_EnumAdapters(dxgiFactory, i, &dxgiAdapter)))
            break;

        if (SUCCEEDED(IDXGIAdapter_GetDesc(dxgiAdapter, &dxgiAdapterDescription)))
        {
            if (RtlIsEqualLuid(&dxgiAdapterDescription.AdapterLuid, &AdapterLuid))
            {
                D3D_FEATURE_LEVEL d3dFeatureLevel[] =
                {
                    D3D_FEATURE_LEVEL_12_1,
                    D3D_FEATURE_LEVEL_12_0,
                    D3D_FEATURE_LEVEL_11_1,
                    D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_10_1,
                    D3D_FEATURE_LEVEL_10_0,
                    D3D_FEATURE_LEVEL_9_3,
                    D3D_FEATURE_LEVEL_9_2,
                    D3D_FEATURE_LEVEL_9_1
                };
                D3D_FEATURE_LEVEL d3ddeviceFeatureLevel;
                ID3D11Device* d3d11device;

                if (D3D11CreateDevice_I && SUCCEEDED(D3D11CreateDevice_I(
                    dxgiAdapter,
                    D3D_DRIVER_TYPE_UNKNOWN,
                    NULL,
                    0,
                    d3dFeatureLevel,
                    RTL_NUMBER_OF(d3dFeatureLevel),
                    D3D11_SDK_VERSION,
                    &d3d11device,
                    &d3ddeviceFeatureLevel,
                    NULL
                    )))
                {
                    d3dFeatureLevelResult = d3ddeviceFeatureLevel;
                    ID3D11Device_Release(d3d11device);
                    IDXGIAdapter_Release(dxgiAdapter);
                    break;
                }
            }
        }

        IDXGIAdapter_Release(dxgiAdapter);
    }

    IDXGIFactory1_Release(dxgiFactory);
    return d3dFeatureLevelResult;
}

NTSTATUS EtQueryAdapterInformation(_In_ D3DKMT_HANDLE AdapterHandle, _In_ KMTQUERYADAPTERINFOTYPE InformationClass,  _Out_writes_bytes_opt_(InformationLength) PVOID Information,  _In_ UINT32 InformationLength)
{
    D3DKMT_QUERYADAPTERINFO queryAdapterInfo;

    memset(&queryAdapterInfo, 0, sizeof(D3DKMT_QUERYADAPTERINFO));

    queryAdapterInfo.AdapterHandle = AdapterHandle;
    queryAdapterInfo.Type = InformationClass;
    queryAdapterInfo.PrivateDriverData = Information;
    queryAdapterInfo.PrivateDriverDataSize = InformationLength;

    return D3DKMTQueryAdapterInfo(&queryAdapterInfo);
}

BOOLEAN EtpIsGpuSoftwareDevice(_In_ D3DKMT_HANDLE AdapterHandle)
{
    D3DKMT_ADAPTERTYPE adapterType;

    memset(&adapterType, 0, sizeof(D3DKMT_ADAPTERTYPE));

    if (NT_SUCCESS(EtQueryAdapterInformation(AdapterHandle, KMTQAITYPE_ADAPTERTYPE, &adapterType, sizeof(D3DKMT_ADAPTERTYPE))))
    {
        if (adapterType.SoftwareDevice) // adapterType.HybridIntegrated
        {
            return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN EtCloseAdapterHandle(_In_ D3DKMT_HANDLE AdapterHandle)
{
    D3DKMT_CLOSEADAPTER closeAdapter;

    memset(&closeAdapter, 0, sizeof(D3DKMT_CLOSEADAPTER));
    closeAdapter.AdapterHandle = AdapterHandle;

    return NT_SUCCESS(D3DKMTCloseAdapter(&closeAdapter));
}

D3DKMT_DRIVERVERSION EtpGetGpuWddmVersion(_In_ D3DKMT_HANDLE AdapterHandle)
{
    D3DKMT_DRIVERVERSION driverVersion;

    memset(&driverVersion, 0, sizeof(D3DKMT_DRIVERVERSION));

    if (NT_SUCCESS(EtQueryAdapterInformation(AdapterHandle, KMTQAITYPE_DRIVERVERSION, &driverVersion, sizeof(D3DKMT_ADAPTERTYPE))))
    {
        return driverVersion;
    }

    return KMT_DRIVERVERSION_WDDM_1_0;
}


SGpuAdapter* CGpuMonitor::AddDisplayAdapter(wchar_t* DeviceInterface, /*D3DKMT_HANDLE*/quint32 AdapterHandle, /*LUID**/struct _LUID* pAdapterLuid, ulong NumberOfSegments, ulong NumberOfNodes)
{
	SGpuAdapter* adapter = new SGpuAdapter();

    adapter->Info.DeviceInterface = QString::fromWCharArray(DeviceInterface);
    adapter->AdapterLuid = *pAdapterLuid;
    adapter->NodeCount = NumberOfNodes;
    adapter->SegmentCount = NumberOfSegments;

	adapter->ApertureBitMapBuffer.fill(0, BYTES_NEEDED_FOR_BITS(NumberOfSegments) / sizeof(ulong));

	adapter->TotalTimeDeltas.resize(NumberOfNodes);

    RtlInitializeBitMap(&adapter->ApertureBitMap, adapter->ApertureBitMapBuffer.data(), NumberOfSegments);

    QString description;
    if (QueryDeviceProperties(DeviceInterface, &description, NULL, NULL, NULL, NULL))
        adapter->Info.Description = description;

    if (WindowsVersion >= WINDOWS_10_RS4) // Note: Changed to RS4 due to reports of BSODs on LTSB versions of RS3
    {
        for (ULONG i = 0; i < adapter->NodeCount; i++)
        {
            D3DKMT_NODEMETADATA metaDataInfo;
            memset(&metaDataInfo, 0, sizeof(D3DKMT_NODEMETADATA));
            metaDataInfo.NodeOrdinal = i;
			if (NT_SUCCESS(EtQueryAdapterInformation(AdapterHandle, KMTQAITYPE_NODEMETADATA, &metaDataInfo, sizeof(D3DKMT_NODEMETADATA))))
				adapter->Info.NodeNameList.append(GetNodeEngineTypeString(&metaDataInfo));
            else
				adapter->Info.NodeNameList.append(QString());
        }
    }

	m_GpuAdapterList.append(adapter);

	return adapter;
}

bool CGpuMonitor::InitializeD3DStatistics()
{
    PPH_LIST deviceAdapterList;
    PWSTR deviceInterfaceList;
    ULONG deviceInterfaceListLength = 0;
    PWSTR deviceInterface;
    D3DKMT_OPENADAPTERFROMDEVICENAME openAdapterFromDeviceName;
    D3DKMT_QUERYSTATISTICS queryStatistics;

    if (CM_Get_Device_Interface_List_Size(&deviceInterfaceListLength, (PGUID)&GUID_DISPLAY_DEVICE_ARRIVAL, NULL, CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS)
        return false;

    deviceInterfaceList = (PWSTR)PhAllocate(deviceInterfaceListLength * sizeof(WCHAR));
    memset(deviceInterfaceList, 0, deviceInterfaceListLength * sizeof(WCHAR));

    if (CM_Get_Device_Interface_List((PGUID)&GUID_DISPLAY_DEVICE_ARRIVAL, NULL, deviceInterfaceList, deviceInterfaceListLength, CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS)
    {
        PhFree(deviceInterfaceList);
        return false;
    }

    deviceAdapterList = PhCreateList(10);

    for (deviceInterface = deviceInterfaceList; *deviceInterface; deviceInterface += PhCountStringZ(deviceInterface) + 1)
    {
        PhAddItemList(deviceAdapterList, deviceInterface);
    }

    for (ULONG i = 0; i < deviceAdapterList->Count; i++)
    {
        memset(&openAdapterFromDeviceName, 0, sizeof(D3DKMT_OPENADAPTERFROMDEVICENAME));
        openAdapterFromDeviceName.DeviceName = (PWSTR)deviceAdapterList->Items[i];

        if (!NT_SUCCESS(D3DKMTOpenAdapterFromDeviceName(&openAdapterFromDeviceName)))
            continue;

        if (WindowsVersion >= WINDOWS_10_RS4 && deviceAdapterList->Count > 1) // Note: Changed to RS4 due to reports of BSODs on LTSB versions of RS3
        {
            if (EtpIsGpuSoftwareDevice(openAdapterFromDeviceName.AdapterHandle))
            {
                EtCloseAdapterHandle(openAdapterFromDeviceName.AdapterHandle);
                continue;
            }
        }

		//
					//m_GpuDedicatedLimit += segmentInfo.DedicatedVideoMemorySize;
					//m_GpuSharedLimit += segmentInfo.SharedSystemMemorySize;
		//

        memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
        queryStatistics.Type = D3DKMT_QUERYSTATISTICS_ADAPTER;
        queryStatistics.AdapterLuid = openAdapterFromDeviceName.AdapterLuid;

        if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
        {
			QWriteLocker Locker(&m_StatsMutex);

            SGpuAdapter* gpuAdapter;

            gpuAdapter = AddDisplayAdapter(openAdapterFromDeviceName.DeviceName, openAdapterFromDeviceName.AdapterHandle, &openAdapterFromDeviceName.AdapterLuid,  
											queryStatistics.QueryResult.AdapterInformation.NbSegments, queryStatistics.QueryResult.AdapterInformation.NodeCount);

			//
			if (WindowsVersion >= WINDOWS_10_RS4) // Note: Changed to RS4 due to reports of BSODs on LTSB versions of RS3
			{
				D3DKMT_SEGMENTSIZEINFO segmentInfo;
				memset(&segmentInfo, 0, sizeof(D3DKMT_SEGMENTSIZEINFO));
				if (NT_SUCCESS(EtQueryAdapterInformation(openAdapterFromDeviceName.AdapterHandle, KMTQAITYPE_GETSEGMENTSIZE, &segmentInfo, sizeof(D3DKMT_SEGMENTSIZEINFO))))
				{
					gpuAdapter->GpuMemory.DedicatedLimit += segmentInfo.DedicatedVideoMemorySize;
					gpuAdapter->GpuMemory.SharedLimit += segmentInfo.SharedSystemMemorySize;
				}
			}
			//

            //gpuAdapter->FirstNodeIndex = m_GpuNextNodeIndex;
            //m_GpuTotalNodeCount += gpuAdapter->NodeCount;
            //m_GpuTotalSegmentCount += gpuAdapter->SegmentCount;
            //m_GpuNextNodeIndex += gpuAdapter->NodeCount;

            for (ULONG ii = 0; ii < gpuAdapter->SegmentCount; ii++)
            {
                memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
                queryStatistics.Type = D3DKMT_QUERYSTATISTICS_SEGMENT;
                queryStatistics.AdapterLuid = openAdapterFromDeviceName.AdapterLuid;
                queryStatistics.QuerySegment.SegmentId = ii;

                if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
                {
                    ULONG64 commitLimit;
                    ULONG aperture;

                    if (WindowsVersion >= WINDOWS_8)
                    {
                        commitLimit = queryStatistics.QueryResult.SegmentInformation.CommitLimit;
                        aperture = queryStatistics.QueryResult.SegmentInformation.Aperture;
                    }
                    else
                    {
                        commitLimit = queryStatistics.QueryResult.SegmentInformationV1.CommitLimit;
                        aperture = queryStatistics.QueryResult.SegmentInformationV1.Aperture;
                    }

                    if (WindowsVersion < WINDOWS_10_RS4) // Note: Changed to RS4 due to reports of BSODs on LTSB versions of RS3
                    {
                        if (aperture)
							gpuAdapter->GpuMemory.SharedLimit += commitLimit;
                            //m_GpuSharedLimit += commitLimit;
                        else
							gpuAdapter->GpuMemory.DedicatedLimit += commitLimit;
                            //m_GpuDedicatedLimit += commitLimit;
                    }

                    if (aperture)
                        RtlSetBits(&gpuAdapter->ApertureBitMap, ii, 1);
                }
            }
        }

        EtCloseAdapterHandle(openAdapterFromDeviceName.AdapterHandle);
    }

    PhDereferenceObject(deviceAdapterList);
    PhFree(deviceInterfaceList);

    //return m_GpuTotalNodeCount > 0;
	return true;
}

void CGpuMonitor::UpdateProcessSegmentInformation(const CProcessPtr& pProcess)
{
	HANDLE QueryHandle = pProcess.objectCast<CWinProcess>()->GetQueryHandle();
    if (!QueryHandle)
        return;

    ULONG64 dedicatedUsage = 0;
    ULONG64 sharedUsage = 0;

    for (ULONG i = 0; i < m_GpuAdapterList.count(); i++)
    {
		SGpuAdapter* gpuAdapter = m_GpuAdapterList.at(i);

        for (ULONG j = 0; j < gpuAdapter->SegmentCount; j++)
        {
			D3DKMT_QUERYSTATISTICS queryStatistics;

            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_PROCESS_SEGMENT;
            queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
            queryStatistics.ProcessHandle = QueryHandle;
            queryStatistics.QueryProcessSegment.SegmentId = j;

            if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
            {
                ULONG64 bytesCommitted;

                if (WindowsVersion >= WINDOWS_8)
                    bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInformation.BytesCommitted;
                else
                    bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInformation.BytesCommitted;

                if (RtlCheckBit(&gpuAdapter->ApertureBitMap, j))
                    sharedUsage += bytesCommitted;
                else
                    dedicatedUsage += bytesCommitted;
            }
        }
    }

	pProcess->SetGpuMemoryUsage(dedicatedUsage, sharedUsage);
}

void CGpuMonitor::UpdateSystemSegmentInformation()
{
    //ULONG64 dedicatedUsage = 0;
    //ULONG64 sharedUsage = 0;

    for (ULONG i = 0; i < m_GpuAdapterList.count(); i++)
    {
		SGpuAdapter* gpuAdapter = m_GpuAdapterList.at(i);

		ULONG64 dedicatedUsage = 0;
		ULONG64 sharedUsage = 0;

        for (ULONG j = 0; j < gpuAdapter->SegmentCount; j++)
        {
			D3DKMT_QUERYSTATISTICS queryStatistics;

            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_SEGMENT;
            queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
            queryStatistics.QuerySegment.SegmentId = j;

            if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
            {
                ULONG64 bytesCommitted;

                if (WindowsVersion >= WINDOWS_8)
                    bytesCommitted = queryStatistics.QueryResult.SegmentInformation.BytesResident;
                else
                    bytesCommitted = queryStatistics.QueryResult.SegmentInformationV1.BytesResident;

                if (RtlCheckBit(&gpuAdapter->ApertureBitMap, j))
                    sharedUsage += bytesCommitted;
                else
                    dedicatedUsage += bytesCommitted;
            }
        }

		gpuAdapter->GpuMemory.DedicatedUsage = dedicatedUsage;
		gpuAdapter->GpuMemory.SharedUsage = sharedUsage;
    }

    //m_GpuDedicatedUsage = dedicatedUsage;
    //m_GpuSharedUsage = sharedUsage;
}

void CGpuMonitor::UpdateProcessNodeInformation(const CProcessPtr& pProcess)
{
	HANDLE QueryHandle = pProcess.objectCast<CWinProcess>()->GetQueryHandle();
    if (!QueryHandle)
        return;

    ULONG64 totalRunningTime = 0;
	
	QStringList CurrentAdapter;

    for (ULONG i = 0; i < m_GpuAdapterList.count(); i++)
    {
		SGpuAdapter* gpuAdapter = m_GpuAdapterList.at(i);

		ULONG64 adapterRunningTime = 0;
        for (ULONG j = 0; j < gpuAdapter->NodeCount; j++)
        {
			D3DKMT_QUERYSTATISTICS queryStatistics;

            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_PROCESS_NODE;
            queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
            queryStatistics.ProcessHandle = QueryHandle;
            queryStatistics.QueryProcessNode.NodeId = j;

            if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
            {
                //ULONG64 runningTime;
                //runningTime = queryStatistics.QueryResult.ProcessNodeInformation.RunningTime.QuadPart;
                //PhUpdateDelta(&Block->GpuTotalRunningTimeDelta[j], runningTime);

				adapterRunningTime += queryStatistics.QueryResult.ProcessNodeInformation.RunningTime.QuadPart;
                totalRunningTime += queryStatistics.QueryResult.ProcessNodeInformation.RunningTime.QuadPart;
            }
        }

		if (adapterRunningTime > 0)
			CurrentAdapter.append(gpuAdapter->Info.Description);
    }

	pProcess->UpdateGpuTimeDelta(totalRunningTime);
	pProcess->SetGpuAdapter(CurrentAdapter.join(","));
}

void CGpuMonitor::UpdateSystemNodeInformation()
{
    for (ULONG i = 0; i < m_GpuAdapterList.count(); i++)
    {
		SGpuAdapter* gpuAdapter = m_GpuAdapterList.at(i);

        for (ULONG j = 0; j < gpuAdapter->NodeCount; j++)
        {
			D3DKMT_QUERYSTATISTICS queryStatistics;

            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_NODE;
            queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
            queryStatistics.QueryNode.NodeId = j;

            if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
            {
                ULONG64 runningTime;
                //ULONG64 systemRunningTime;

                runningTime = queryStatistics.QueryResult.NodeInformation.GlobalInformation.RunningTime.QuadPart;
                //systemRunningTime = queryStatistics.QueryResult.NodeInformation.SystemInformation.RunningTime.QuadPart;

				gpuAdapter->TotalTimeDeltas[j].Update(runningTime);
				//m_GpuNodesTotalRunningTimeDeltas[gpuAdapter->FirstNodeIndex + j].Update(runningTime);
            }
        }
    }

	LARGE_INTEGER performanceCounter, performanceFrequency;
    NtQueryPerformanceCounter(&performanceCounter, &performanceFrequency);
	m_ClockTotalRunningTimeDelta.Update(performanceCounter.QuadPart);
	m_ClockTotalRunningTimeFrequency = performanceFrequency.QuadPart;
}

bool CGpuMonitor::UpdateSystemStats()
{
	// Update global statistics.
	{
		QWriteLocker Locker(&m_StatsMutex);
		UpdateSystemSegmentInformation();
		UpdateSystemNodeInformation();
	}

	// Update global gpu usage. 
	// total GPU node elapsed time in micro-seconds
	double elapsedTime = (DOUBLE)(m_ClockTotalRunningTimeDelta.Delta * 10000000ULL / m_ClockTotalRunningTimeFrequency);

	if (elapsedTime != 0)
	{
		QWriteLocker Locker(&m_StatsMutex);

		for (ULONG i = 0; i < m_GpuAdapterList.count(); i++)
		{
			SGpuAdapter* gpuAdapter = m_GpuAdapterList.at(i);

			float tempGpuUsage = 0;

			for (ULONG j = 0; j < gpuAdapter->NodeCount; j++)
			{
				FLOAT usage = (float)(gpuAdapter->TotalTimeDeltas[j].Delta / elapsedTime);

				if (usage > 1)
					usage = 1;

				if (usage > tempGpuUsage)
					tempGpuUsage = usage;
			}

			gpuAdapter->TimeUsage = tempGpuUsage;
		}

		/*for (quint32 i = 0; i < m_GpuTotalNodeCount; i++)
		{
			FLOAT usage = (float)(m_GpuNodesTotalRunningTimeDeltas[i].Delta / elapsedTime);

			if (usage > 1)
				usage = 1;

			if (usage > tempGpuUsage)
				tempGpuUsage = usage;
		}*/
	}

	//m_GpuNodeUsage = tempGpuUsage;


	// Update per-process statistics.
	QMap<quint64, CProcessPtr> ProcessList = theAPI->GetProcessList();

	foreach(const CProcessPtr& pProcess, ProcessList)
	{
		QWriteLocker Locker(&m_StatsMutex);

		UpdateProcessSegmentInformation(pProcess);
        UpdateProcessNodeInformation(pProcess);

		pProcess->UpdateGpuTimeUsage(elapsedTime);
	}

	return true;
}

CGpuMonitor::SGpuInfo CGpuMonitor::GetGpuInfo(int Index) 
{ 
	QReadLocker Locker(&m_StatsMutex); 
	if (Index >= m_GpuAdapterList.size())
		return SGpuInfo();
	return m_GpuAdapterList.at(Index)->Info;
}

float CGpuMonitor::GetGpuUsage(int Index)
{
	QReadLocker Locker(&m_StatsMutex); 
	if (Index >= m_GpuAdapterList.size())
		return 0.0;
	return m_GpuAdapterList[Index]->TimeUsage;
}

CGpuMonitor::SGpuMemory CGpuMonitor::GetGpuMemory(int Index)
{
	QReadLocker Locker(&m_StatsMutex); 
	if (Index >= m_GpuAdapterList.size())
		return SGpuMemory();
	return m_GpuAdapterList[Index]->GpuMemory;
}

CGpuMonitor::SGpuMemory CGpuMonitor::GetGpuMemory()
{
	SGpuMemory TotalMemory;

	QReadLocker Locker(&m_StatsMutex); 
	for (ULONG i = 0; i < m_GpuAdapterList.count(); i++)
	{
		SGpuAdapter* gpuAdapter = m_GpuAdapterList.at(i);

		TotalMemory.DedicatedLimit += gpuAdapter->GpuMemory.DedicatedLimit;
		TotalMemory.DedicatedUsage += gpuAdapter->GpuMemory.DedicatedUsage;
		TotalMemory.SharedLimit += gpuAdapter->GpuMemory.SharedLimit;
		TotalMemory.SharedUsage += gpuAdapter->GpuMemory.SharedUsage;
	}

	return TotalMemory;
}

/*
typedef struct _ET_PROCESS_GPU_STATISTICS
{
    ULONG SegmentCount;
    ULONG NodeCount;

    ULONG64 DedicatedCommitted;
    ULONG64 SharedCommitted;

    ULONG64 BytesAllocated;
    ULONG64 BytesReserved;
    ULONG64 WriteCombinedBytesAllocated;
    ULONG64 WriteCombinedBytesReserved;
    ULONG64 CachedBytesAllocated;
    ULONG64 CachedBytesReserved;
    ULONG64 SectionBytesAllocated;
    ULONG64 SectionBytesReserved;

    ULONG64 RunningTime;
    ULONG64 ContextSwitches;
} ET_PROCESS_GPU_STATISTICS, *PET_PROCESS_GPU_STATISTICS;

VOID EtQueryProcessGpuStatistics(_In_ HANDLE ProcessHandle, _Out_ PET_PROCESS_GPU_STATISTICS Statistics)
{
    ULONG i;
    ULONG j;
    PETP_GPU_ADAPTER gpuAdapter;
    D3DKMT_QUERYSTATISTICS queryStatistics;

    memset(Statistics, 0, sizeof(ET_PROCESS_GPU_STATISTICS));

    for (i = 0; i < EtpGpuAdapterList->Count; i++)
    {
        gpuAdapter = (PETP_GPU_ADAPTER)EtpGpuAdapterList->Items[i];

        Statistics->SegmentCount += gpuAdapter->SegmentCount;
        Statistics->NodeCount += gpuAdapter->NodeCount;

        for (j = 0; j < gpuAdapter->SegmentCount; j++)
        {
            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_PROCESS_SEGMENT;
            queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
            queryStatistics.ProcessHandle = ProcessHandle;
            queryStatistics.QueryProcessSegment.SegmentId = j;

            if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
            {
                ULONG64 bytesCommitted;

                if (WindowsVersion >= WINDOWS_8)
                    bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInformation.BytesCommitted;
                else
                    bytesCommitted = queryStatistics.QueryResult.ProcessSegmentInformation.BytesCommitted;

                if (RtlCheckBit(&gpuAdapter->ApertureBitMap, j))
                    Statistics->SharedCommitted += bytesCommitted;
                else
                    Statistics->DedicatedCommitted += bytesCommitted;
            }
        }

        for (j = 0; j < gpuAdapter->NodeCount; j++)
        {
            memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
            queryStatistics.Type = D3DKMT_QUERYSTATISTICS_PROCESS_NODE;
            queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
            queryStatistics.ProcessHandle = ProcessHandle;
            queryStatistics.QueryProcessNode.NodeId = j;

            if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
            {
                Statistics->RunningTime += queryStatistics.QueryResult.ProcessNodeInformation.RunningTime.QuadPart;
                Statistics->ContextSwitches += queryStatistics.QueryResult.ProcessNodeInformation.ContextSwitch;
            }
        }

        memset(&queryStatistics, 0, sizeof(D3DKMT_QUERYSTATISTICS));
        queryStatistics.Type = D3DKMT_QUERYSTATISTICS_PROCESS;
        queryStatistics.AdapterLuid = gpuAdapter->AdapterLuid;
        queryStatistics.ProcessHandle = ProcessHandle;

        if (NT_SUCCESS(D3DKMTQueryStatistics(&queryStatistics)))
        {
            Statistics->BytesAllocated += queryStatistics.QueryResult.ProcessInformation.SystemMemory.BytesAllocated;
            Statistics->BytesReserved += queryStatistics.QueryResult.ProcessInformation.SystemMemory.BytesReserved;
            Statistics->WriteCombinedBytesAllocated += queryStatistics.QueryResult.ProcessInformation.SystemMemory.WriteCombinedBytesAllocated;
            Statistics->WriteCombinedBytesReserved += queryStatistics.QueryResult.ProcessInformation.SystemMemory.WriteCombinedBytesReserved;
            Statistics->CachedBytesAllocated += queryStatistics.QueryResult.ProcessInformation.SystemMemory.CachedBytesAllocated;
            Statistics->CachedBytesReserved += queryStatistics.QueryResult.ProcessInformation.SystemMemory.CachedBytesReserved;
            Statistics->SectionBytesAllocated += queryStatistics.QueryResult.ProcessInformation.SystemMemory.SectionBytesAllocated;
            Statistics->SectionBytesReserved += queryStatistics.QueryResult.ProcessInformation.SystemMemory.SectionBytesReserved;
        }
    }
}*/