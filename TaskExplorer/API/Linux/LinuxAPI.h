#pragma once

#include "../SystemAPI.h"
#include "LinuxProcess.h"
#include "LinuxSocket.h"
#include "LinuxService.h"
#include "LinuxDriver.h"
#include "ProcFs.h"
#include "UdevMonitor.h"

//
// The Linux system backend.
//
// This is the counterpart of CWindowsAPI and the single entry point the rest of
// the application uses; CSystemAPI::InitAPI() picks between the two.
//
// Data sources, by area:
//   processes  /proc/<pid>/{stat,statm,status,cmdline,exe,cwd,io}
//   threads    /proc/<pid>/task/<tid>/{stat,comm}
//   handles    /proc/<pid>/fd + fdinfo
//   sockets    sock_diag netlink, falling back to /proc/net/*
//   services   systemd over the D-Bus system bus
//   drivers    /proc/modules + /sys/module
//   sys stats  /proc/{stat,meminfo,uptime,loadavg}
//
class CLinuxAPI : public CSystemAPI
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxAPI)
public:
	CLinuxAPI(QObject *parent = nullptr);
	virtual ~CLinuxAPI();

	virtual bool			RootAvaiable();

	virtual bool			UpdateAll();
	virtual bool			UpdateSysStats();
	virtual bool			UpdateProcessList();
	virtual bool			UpdateSocketList();
	virtual bool			UpdateOpenFileList();
	virtual bool			UpdateServiceList(bool bRefresh = false);
	virtual bool			UpdateDriverList();

	//
	// Fetches, in one batched request to an elevated TaskHelper, the I/O counters
	// of processes whose /proc/<pid>/io this user cannot read.
	//
	void					UpdateHelperProcIo();

	virtual void			ClearPersistence();

	virtual quint64			GetUpTime() const;
	virtual QList<SUser>	GetUsers() const;

	virtual QMultiMap<QString, CDnsCacheEntryPtr>	GetDnsEntryList() const;
	virtual bool			UpdateDnsCache();
	virtual void			FlushDnsCache();

	//
	// Permits one further interactive polkit prompt for the cache dump.
	//
	// Reading the cache is an auth_admin_keep action, so the prompt has to be
	// rationed: the view refreshes on a timer and must not raise a password
	// dialog every few seconds. But a prompt that was cancelled should be
	// retryable without restarting the application.
	//
	// The DNS cache view calls this when it becomes visible, which makes
	// "navigate back to the tab" the way to try again. Once authentication has
	// succeeded no prompt appears at all, because the non-interactive call keeps
	// working for as long as polkit remembers it.
	//
	void					AllowDnsAuthPrompt()	{ m_bDnsAuthAttempted = false; }

	// ---- Linux specifics ----

	//
	// Pressure Stall Information: the share of time work was stalled waiting
	// for cpu, memory or I/O. Valid is false when the kernel does not provide
	// it, which callers should treat as "nothing to show" rather than zero.
	//
	virtual ProcFs::SPressure	GetCpuPressure() const		{ QReadLocker Locker(&m_StatsMutex); return m_CpuPressure; }
	virtual ProcFs::SPressure	GetMemoryPressure() const	{ QReadLocker Locker(&m_StatsMutex); return m_MemoryPressure; }
	virtual ProcFs::SPressure	GetIoPressure() const		{ QReadLocker Locker(&m_StatsMutex); return m_IoPressure; }

	// Distribution name and kernel release, for the system info panel.
	virtual QString			GetDistroName() const	{ QReadLocker Locker(&m_Mutex); return m_SystemName; }
	virtual QString			GetKernelVersion() const { QReadLocker Locker(&m_Mutex); return m_SystemVersion; }

	// Whether this build can see other users' processes in full detail, which
	// depends on being root or holding CAP_SYS_PTRACE.
	virtual bool			HasFullProcessAccess() const { return m_bFullAccess; }

private slots:
	virtual bool			Init();
	virtual void			OnHardwareChanged();

protected:
	// Reads the one-shot host facts: hostname, distro, kernel, cpu model and
	// topology, installed memory.
	virtual bool			InitSystemInfo();
	virtual bool			InitCpuInfo();

	// Samples /proc/stat and updates the aggregate and per-cpu usage figures.
	// Returns the total cpu ticks elapsed since the previous sample, which is
	// the divisor for per-process cpu percentages.
	virtual quint64			UpdateCpuStats();

	// Samples /proc/meminfo and /proc/swaps into the m_*Memory fields.
	virtual void			UpdateMemStats();

	// Previous /proc/stat sample, for the cpu usage deltas.
	ProcFs::SSysStat		m_LastSysStat;

	bool					m_bFullAccess;

	// Hotplug watch; null when the uevent socket could not be opened.
	class CUdevMonitor*	m_pUdevMonitor;

	//
	// Running totals of the per-process logical I/O deltas; see UpdateSysStats
	// for why these accumulate rather than being summed fresh each cycle.
	//
	quint64					m_TotalIoRead = 0;
	quint64					m_TotalIoReadOps = 0;
	quint64					m_TotalIoWrite = 0;
	quint64					m_TotalIoWriteOps = 0;

	//
	// System-wide Pressure Stall Information, sampled in UpdateSysStats.
	// Guarded by m_StatsMutex like the other counters.
	//
	ProcFs::SPressure		m_CpuPressure;
	ProcFs::SPressure		m_MemoryPressure;
	ProcFs::SPressure		m_IoPressure;

	//
	// The DNS cache, as last read from systemd-resolved. Keyed by host name,
	// multi-valued because one name commonly has several records.
	//
	mutable QReadWriteLock	m_DnsMutex;
	QMultiMap<QString, CDnsCacheEntryPtr>	m_DnsCache;

	// Whether the one interactive polkit prompt for the cache dump has already
	// been offered this run; see UpdateDnsCache.
	bool					m_bDnsAuthAttempted;
};

//
// The Windows backend measures cpu time in 100ns units; on Linux the /proc
// counters are in clock ticks, so this is sysconf(_SC_CLK_TCK) rather than a
// fixed constant.
//
#define CPU_TIME_DIVIDER (ProcFs::ClockTicksPerSec())
