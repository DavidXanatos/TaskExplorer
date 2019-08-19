#pragma once

// adapter.c

typedef struct _DV_NETADAPTER_ID
{
    NET_IFINDEX InterfaceIndex;
    IF_LUID InterfaceLuid;
    PPH_STRING InterfaceGuid;
    PPH_STRING InterfaceDevice;
} DV_NETADAPTER_ID, *PDV_NETADAPTER_ID;

// ndis.c

#define BITS_IN_ONE_BYTE 8
#define NDIS_UNIT_OF_MEASUREMENT 100

// dmex: rev
typedef ULONG (WINAPI* _GetInterfaceDescriptionFromGuid)(
    _Inout_ PGUID InterfaceGuid,
    _Out_opt_ PWSTR InterfaceDescription,
    _Inout_ PSIZE_T LengthAddress,
    PVOID Unknown1,
    PVOID Unknown2
    );

BOOLEAN NetworkAdapterQuerySupported(
    _In_ HANDLE DeviceHandle
    );

BOOLEAN NetworkAdapterQueryNdisVersion(
    _In_ HANDLE DeviceHandle,
    _Out_opt_ PUINT MajorVersion,
    _Out_opt_ PUINT MinorVersion
    );

PPH_STRING NetworkAdapterQueryName(
    _In_ HANDLE DeviceHandle,
    _In_ PPH_STRING InterfaceGuid
    );

NTSTATUS NetworkAdapterQueryStatistics(
    _In_ HANDLE DeviceHandle,
    _Out_ PNDIS_STATISTICS_INFO Info
    );

NTSTATUS NetworkAdapterQueryLinkState(
    _In_ HANDLE DeviceHandle,
    _Out_ PNDIS_LINK_STATE State
    );

BOOLEAN NetworkAdapterQueryMediaType(
    _In_ HANDLE DeviceHandle,
    _Out_ PNDIS_PHYSICAL_MEDIUM Medium
    );

NTSTATUS NetworkAdapterQueryLinkSpeed(
    _In_ HANDLE DeviceHandle,
    _Out_ PULONG64 LinkSpeed
    );

ULONG64 NetworkAdapterQueryValue(
    _In_ HANDLE DeviceHandle,
    _In_ NDIS_OID OpCode
    );

BOOLEAN QueryInterfaceRow(
    _In_ PDV_NETADAPTER_ID Id,
    _Out_ PMIB_IF_ROW2 InterfaceRow
    );

PWSTR MediumTypeToString(
    _In_ NDIS_PHYSICAL_MEDIUM MediumType
    );

// storage.c

PPH_STRING DiskDriveQueryDosMountPoints(
    _In_ ULONG DeviceNumber
    );

typedef struct _DISK_HANDLE_ENTRY
{
    WCHAR DeviceLetter;
    HANDLE DeviceHandle;
} DISK_HANDLE_ENTRY, *PDISK_HANDLE_ENTRY;

PPH_LIST DiskDriveQueryMountPointHandles(
    _In_ ULONG DeviceNumber
    );

BOOLEAN DiskDriveQueryDeviceInformation(
    _In_ HANDLE DeviceHandle,
    _Out_opt_ PPH_STRING* DiskVendor,
    _Out_opt_ PPH_STRING* DiskModel,
    _Out_opt_ PPH_STRING* DiskRevision,
    _Out_opt_ PPH_STRING* DiskSerial
    );

NTSTATUS DiskDriveQueryDeviceTypeAndNumber(
    _In_ HANDLE DeviceHandle,
    _Out_opt_ PULONG DeviceNumber,
    _Out_opt_ DEVICE_TYPE* DeviceType
    );

NTSTATUS DiskDriveQueryStatistics(
    _In_ HANDLE DeviceHandle,
    _Out_ PDISK_PERFORMANCE Info
    );

PPH_STRING DiskDriveQueryGeometry(
    _In_ HANDLE DeviceHandle
    );

ULONG64 DiskDriveQuerySize(
    _In_ HANDLE DeviceHandle
    );

NTSTATUS DiskDriveQueryImminentFailure(
    _In_ HANDLE DeviceHandle,
    _Out_ PPH_LIST* DiskSmartAttributes
    );

typedef struct _NTFS_FILESYSTEM_STATISTICS
{
    FILESYSTEM_STATISTICS FileSystemStatistics;
    NTFS_STATISTICS NtfsStatistics;
} NTFS_FILESYSTEM_STATISTICS, *PNTFS_FILESYSTEM_STATISTICS;

typedef struct _FAT_FILESYSTEM_STATISTICS
{
    FILESYSTEM_STATISTICS FileSystemStatistics;
    NTFS_STATISTICS FatStatistics;
} FAT_FILESYSTEM_STATISTICS, *PFAT_FILESYSTEM_STATISTICS;

typedef struct _EXFAT_FILESYSTEM_STATISTICS
{
    FILESYSTEM_STATISTICS FileSystemStatistics;
    EXFAT_STATISTICS ExFatStatistics;
} EXFAT_FILESYSTEM_STATISTICS, *PEXFAT_FILESYSTEM_STATISTICS;

BOOLEAN DiskDriveQueryFileSystemInfo(
    _In_ HANDLE DeviceHandle,
    _Out_ USHORT* FileSystemType,
    _Out_ PVOID* FileSystemStatistics
    );

typedef struct _NTFS_VOLUME_INFO
{
    NTFS_VOLUME_DATA_BUFFER VolumeData;
    NTFS_EXTENDED_VOLUME_DATA ExtendedVolumeData;
} NTFS_VOLUME_INFO, *PNTFS_VOLUME_INFO;

BOOLEAN DiskDriveQueryNtfsVolumeInfo(
    _In_ HANDLE DosDeviceHandle,
    _Out_ PNTFS_VOLUME_INFO VolumeInfo
    );

BOOLEAN DiskDriveQueryRefsVolumeInfo(
    _In_ HANDLE DosDeviceHandle,
    _Out_ PREFS_VOLUME_DATA_BUFFER VolumeInfo
    );

NTSTATUS DiskDriveQueryVolumeInformation(
    _In_ HANDLE DosDeviceHandle,
    _Out_ PFILE_FS_VOLUME_INFORMATION* VolumeInfo
    );

NTSTATUS DiskDriveQueryVolumeAttributes(
    _In_ HANDLE DosDeviceHandle,
    _Out_ PFILE_FS_ATTRIBUTE_INFORMATION* AttributeInfo
    );

// https://en.wikipedia.org/wiki/S.M.A.R.T.#Known_ATA_S.M.A.R.T._attributes
typedef enum _SMART_ATTRIBUTE_ID
{
    SMART_ATTRIBUTE_ID_READ_ERROR_RATE = 0x01,
    SMART_ATTRIBUTE_ID_THROUGHPUT_PERFORMANCE = 0x02,
    SMART_ATTRIBUTE_ID_SPIN_UP_TIME = 0x03,
    SMART_ATTRIBUTE_ID_START_STOP_COUNT = 0x04,
    SMART_ATTRIBUTE_ID_REALLOCATED_SECTORS_COUNT = 0x05,
    SMART_ATTRIBUTE_ID_READ_CHANNEL_MARGIN = 0x06,
    SMART_ATTRIBUTE_ID_SEEK_ERROR_RATE = 0x07,
    SMART_ATTRIBUTE_ID_SEEK_TIME_PERFORMANCE = 0x08,
    SMART_ATTRIBUTE_ID_POWER_ON_HOURS = 0x09,
    SMART_ATTRIBUTE_ID_SPIN_RETRY_COUNT = 0x0A,
    SMART_ATTRIBUTE_ID_CALIBRATION_RETRY_COUNT = 0x0B,
    SMART_ATTRIBUTE_ID_POWER_CYCLE_COUNT = 0x0C,
    SMART_ATTRIBUTE_ID_SOFT_READ_ERROR_RATE = 0x0D,
    // TO-DO: Add values 14-170
    SMART_ATTRIBUTE_ID_SSD_PROGRAM_FAIL_COUNT = 0xAB,
    SMART_ATTRIBUTE_ID_SSD_ERASE_FAIL_COUNT = 0xAC,
    SMART_ATTRIBUTE_ID_SSD_WEAR_LEVELING_COUNT = 0xAD,
    SMART_ATTRIBUTE_ID_UNEXPECTED_POWER_LOSS = 0xAE,
    // TO-DO: Add values 175-176
    SMART_ATTRIBUTE_ID_WEAR_RANGE_DELTA = 0xB1,
    // TO-DO: Add values 178-180
    SMART_ATTRIBUTE_ID_SSD_PROGRAM_FAIL_COUNT_TOTAL = 0xB5,
    SMART_ATTRIBUTE_ID_ERASE_FAIL_COUNT = 0xB6,
    SMART_ATTRIBUTE_ID_SATA_DOWNSHIFT_ERROR_COUNT = 0xB7,
    SMART_ATTRIBUTE_ID_END_TO_END_ERROR = 0xB8,
    SMART_ATTRIBUTE_ID_HEAD_STABILITY = 0xB9,
    SMART_ATTRIBUTE_ID_INDUCED_OP_VIBRATION_DETECTION = 0xBA,
    SMART_ATTRIBUTE_ID_REPORTED_UNCORRECTABLE_ERRORS = 0xBB,
    SMART_ATTRIBUTE_ID_COMMAND_TIMEOUT = 0xBC,
    SMART_ATTRIBUTE_ID_HIGH_FLY_WRITES = 0xBD,
    SMART_ATTRIBUTE_ID_TEMPERATURE_DIFFERENCE_FROM_100 = 0xBE, // AirflowTemperature
    SMART_ATTRIBUTE_ID_GSENSE_ERROR_RATE = 0xBF,
    SMART_ATTRIBUTE_ID_POWER_OFF_RETRACT_COUNT = 0xC0,
    SMART_ATTRIBUTE_ID_LOAD_CYCLE_COUNT = 0xC1,
    SMART_ATTRIBUTE_ID_TEMPERATURE = 0xC2,
    SMART_ATTRIBUTE_ID_HARDWARE_ECC_RECOVERED = 0xC3,
    SMART_ATTRIBUTE_ID_REALLOCATION_EVENT_COUNT = 0xC4,
    SMART_ATTRIBUTE_ID_CURRENT_PENDING_SECTOR_COUNT = 0xC5,
    SMART_ATTRIBUTE_ID_UNCORRECTABLE_SECTOR_COUNT = 0xC6,
    SMART_ATTRIBUTE_ID_ULTRADMA_CRC_ERROR_COUNT = 0xC7,
    SMART_ATTRIBUTE_ID_MULTI_ZONE_ERROR_RATE = 0xC8,
    SMART_ATTRIBUTE_ID_OFFTRACK_SOFT_READ_ERROR_RATE = 0xC9,
    SMART_ATTRIBUTE_ID_DATA_ADDRESS_MARK_ERRORS = 0xCA,
    SMART_ATTRIBUTE_ID_RUN_OUT_CANCEL = 0xCB,
    SMART_ATTRIBUTE_ID_SOFT_ECC_CORRECTION = 0xCC,
    SMART_ATTRIBUTE_ID_THERMAL_ASPERITY_RATE_TAR = 0xCD,
    SMART_ATTRIBUTE_ID_FLYING_HEIGHT = 0xCE,
    SMART_ATTRIBUTE_ID_SPIN_HIGH_CURRENT = 0xCF,
    SMART_ATTRIBUTE_ID_SPIN_BUZZ = 0xD0,
    SMART_ATTRIBUTE_ID_OFFLINE_SEEK_PERFORMANCE = 0xD1,
    SMART_ATTRIBUTE_ID_VIBRATION_DURING_WRITE = 0xD3,
    SMART_ATTRIBUTE_ID_SHOCK_DURING_WRITE = 0xD4,
    SMART_ATTRIBUTE_ID_DISK_SHIFT = 0xDC,
    SMART_ATTRIBUTE_ID_GSENSE_ERROR_RATE_ALT = 0xDD,
    SMART_ATTRIBUTE_ID_LOADED_HOURS = 0xDE,
    SMART_ATTRIBUTE_ID_LOAD_UNLOAD_RETRY_COUNT = 0xDF,
    SMART_ATTRIBUTE_ID_LOAD_FRICTION = 0xE0,
    SMART_ATTRIBUTE_ID_LOAD_UNLOAD_CYCLE_COUNT = 0xE1,
    SMART_ATTRIBUTE_ID_LOAD_IN_TIME = 0xE2,
    SMART_ATTRIBUTE_ID_TORQUE_AMPLIFICATION_COUNT = 0xE3,
    SMART_ATTRIBUTE_ID_POWER_OFF_RETTRACT_CYCLE = 0xE4,
    // TO-DO: Add value 229
    SMART_ATTRIBUTE_ID_GMR_HEAD_AMPLITUDE = 0xE6,
    SMART_ATTRIBUTE_ID_DRIVE_TEMPERATURE = 0xE7,
    // TO-DO: Add value 232
    SMART_ATTRIBUTE_ID_SSD_MEDIA_WEAROUT_HOURS = 0xE9,
    SMART_ATTRIBUTE_ID_SSD_ERASE_COUNT = 0xEA,
    // TO-DO: Add values 235-239
    SMART_ATTRIBUTE_ID_HEAD_FLYING_HOURS = 0xF0,
    SMART_ATTRIBUTE_ID_TOTAL_LBA_WRITTEN = 0xF1,
    SMART_ATTRIBUTE_ID_TOTAL_LBA_READ = 0xF2,
    // TO-DO: Add values 243-249
    SMART_ATTRIBUTE_ID_READ_ERROR_RETY_RATE = 0xFA,
    SMART_ATTRIBUTE_ID_MIN_SPARES_REMAINING = 0xFB,
    SMART_ATTRIBUTE_ID_NEWLY_ADDED_BAD_FLASH_BLOCK = 0xFC,
    // TO-DO: Add value 253
    SMART_ATTRIBUTE_ID_FREE_FALL_PROTECTION = 0xFE,
} SMART_ATTRIBUTE_ID;


#define SMART_HEADER_SIZE 2

#include <pshpack1.h>
typedef struct _SMART_ATTRIBUTE
{
    BYTE Id;
    USHORT Flags;
    BYTE CurrentValue;
    BYTE WorstValue;
    BYTE RawValue[6];
    BYTE Reserved;
} SMART_ATTRIBUTE, *PSMART_ATTRIBUTE;
#include <poppack.h>

typedef struct _SMART_ATTRIBUTES
{
    SMART_ATTRIBUTE_ID AttributeId;
    UINT CurrentValue;
    UINT WorstValue;
    UINT RawValue;

    // Pre-fail/Advisory bit
    // This bit is applicable only when the value of this attribute is less than or equal to its threshhold.
    // 0 : Advisory: The device has exceeded its intended design life period.
    // 1 : Pre-failure notification : Failure is predicted within 24 hours.
    BOOLEAN Advisory;
    BOOLEAN FailureImminent;

    // Online data collection bit
    // 0 : This value of this attribute is only updated during offline activities.
    // 1 : The value of this attribute is updated during both normal operation and offline activities.
    BOOLEAN OnlineDataCollection;

    // TRUE: This attribute characterizes a performance aspect of the drive,
    //   degradation of which may indicate imminent drive failure, such as data throughput, seektimes, spin up time, etc.
    BOOLEAN Performance;

    // TRUE: This attribute is based on the expected, non-fatal errors that are inherent in disk drives,
    //    increases in which may indicate imminent drive failure, such as ECC errors, seek errors, etc.
    BOOLEAN ErrorRate;

    // TRUE: This attribute counts events, of which an excessive number of which may
    //       indicate imminent drive failure, such as number of re-allocated sectors, etc.
    BOOLEAN EventCount;

    // TRUE: This type is used to specify an attribute that is collected and saved by the drive automatically.
    BOOLEAN SelfPreserving;
} SMART_ATTRIBUTES, *PSMART_ATTRIBUTES;

PWSTR SmartAttributeGetText(
    _In_ SMART_ATTRIBUTE_ID AttributeId
    );

