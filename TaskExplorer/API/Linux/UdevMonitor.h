#pragma once

#include <qobject.h>

class QSocketNotifier;

//
// Watches for hardware hotplug events so the disk, network and GPU lists can be
// re-enumerated when something is added or removed.
//
// This reads the kernel's uevent netlink socket directly rather than linking
// libudev: the events are plain NUL-separated KEY=VALUE records, only a couple
// of keys are needed, and it avoids a build dependency for something this
// small. It also keeps working on a system with no udevd running.
//
// Two multicast groups carry these:
//
//   group 1  raw kernel uevents, emitted whether or not udevd is running
//   group 2  the same events after udevd has processed them, wrapped in a
//            libudev header
//
// Group 1 is preferred; group 2 is the fallback for kernels that restrict it.
// Parsing works for both, because the property block is NUL-separated
// KEY=VALUE either way - only the header differs, and this never reads it.
//
class CUdevMonitor : public QObject
{
	Q_OBJECT

public:
	CUdevMonitor(QObject* parent = nullptr);
	virtual ~CUdevMonitor();

	// Opens the netlink socket and starts watching. Returns false when no
	// group could be bound, in which case no events will ever be emitted and
	// hotplugged devices are simply picked up at the next manual refresh.
	bool			Init();

	bool			IsActive() const { return m_Socket != -1; }

signals:
	//
	// Emitted when a device in a subsystem TaskExplorer cares about appears or
	// disappears. Already coalesced: a single udev "action" can produce a burst
	// of events, and CSystemAPI::NotifyHardwareChanged() debounces further.
	//
	void			HardwareChanged();

private slots:
	void			OnReadyRead();

private:
	int				m_Socket;
	QSocketNotifier*	m_pNotifier;
};
