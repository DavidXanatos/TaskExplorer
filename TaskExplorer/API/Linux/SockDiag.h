#pragma once

#include <qobject.h>
#include <QList>

#include "ProcFs.h"

//
// Socket enumeration over the sock_diag netlink interface.
//
// This is what ss(8) uses, and it is strictly better than parsing
// /proc/net/{tcp,udp}:
//
//   - it reports per-socket cumulative byte counters (via the INET_DIAG_INFO
//     extension, which carries a tcp_info), where /proc/net only exposes the
//     current send/receive queue depths. Without those counters the per-socket
//     transfer rate columns can only ever read zero.
//   - it is a single binary dump rather than a text file that has to be
//     re-parsed per address family.
//   - /proc/net/tcp is O(n^2) to read for large socket counts, because each
//     read() restarts the seq_file walk.
//
// It can fail where /proc/net succeeds - a kernel built without
// CONFIG_INET_DIAG, or a restricted container - so ProcFs::ReadNetConnections()
// remains as a fallback. IsAvailable() reports which path will be taken.
//
namespace SockDiag
{
	// Probes once whether a sock_diag dump can be performed at all.
	bool	IsAvailable();

	//
	// Enumerates TCP and UDP sockets over both address families.
	//
	// The returned records use the same SNetConnection shape as the /proc/net
	// path, with the byte counters filled in where the kernel reported them.
	// Returns an empty list on failure, which the caller should treat as
	// "fall back to /proc/net" rather than "no sockets".
	//
	QList<ProcFs::SNetConnection>	Enumerate();
}
