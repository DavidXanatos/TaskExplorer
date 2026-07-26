#include "stdafx.h"
#include "SockDiag.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>
#include <linux/tcp.h>

namespace SockDiag
{

//
// NET_TYPE_* values, mirrored from SocketInfo.h so this stays independent of
// the API layer (ProcFs does the same).
//
static const quint32 NET_IPV4 = 0x1, NET_IPV6 = 0x2, NET_TCP = 0x10, NET_UDP = 0x20;

//
// The TCP_* state values are spelled out rather than taken from a header:
// glibc puts them in <netinet/tcp.h> behind __USE_MISC, and that header
// conflicts with <linux/tcp.h>, which is needed here for tcp_info. They are
// part of the kernel ABI and cannot change.
//
enum ELinuxTcpState
{
	LinuxTcpEstablished = 1,
	LinuxTcpSynSent     = 2,
	LinuxTcpSynRecv     = 3,
	LinuxTcpFinWait1    = 4,
	LinuxTcpFinWait2    = 5,
	LinuxTcpTimeWait    = 6,
	LinuxTcpClose       = 7,
	LinuxTcpCloseWait   = 8,
	LinuxTcpLastAck     = 9,
	LinuxTcpListen      = 10,
	LinuxTcpClosing     = 11,
};

//
// Kernel TCP state -> MIB_TCP_STATE, the numbering the shared socket layer
// renders. The two do not agree: Linux ESTABLISHED is 1, the MIB value is 5.
//
static quint32 TcpStateToMib(quint32 LinuxState)
{
	switch (LinuxState)
	{
		case LinuxTcpEstablished: return 5;
		case LinuxTcpSynSent:     return 3;
		case LinuxTcpSynRecv:     return 4;
		case LinuxTcpFinWait1:    return 6;
		case LinuxTcpFinWait2:    return 7;
		case LinuxTcpTimeWait:    return 11;
		case LinuxTcpClose:       return 1;
		case LinuxTcpCloseWait:   return 8;
		case LinuxTcpLastAck:     return 10;
		case LinuxTcpListen:      return 2;
		case LinuxTcpClosing:     return 9;
	}
	return 0;
}

//
// idiag_src/idiag_dst are always in network byte order and always 4 words wide,
// of which only the first is used for IPv4.
//
static QHostAddress ToAddress(const __be32* pRaw, bool bIPv6)
{
	if (!bIPv6)
		return QHostAddress(qFromBigEndian<quint32>(pRaw[0]));

	quint8 Bytes[16];
	memcpy(Bytes, pRaw, 16);
	return QHostAddress(Bytes);
}

//
// Performs one dump request for a (family, protocol) pair and appends the
// results. Returns false if the request itself could not be issued, which is
// how the caller detects that sock_diag is unavailable.
//
static bool DumpOne(int Family, int Protocol, QList<ProcFs::SNetConnection>& Connections)
{
	const int Socket = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_SOCK_DIAG);
	if (Socket < 0)
		return false;

	struct
	{
		nlmsghdr Header;
		inet_diag_req_v2 Request;
	} Query;

	memset(&Query, 0, sizeof(Query));
	Query.Header.nlmsg_len = sizeof(Query);
	Query.Header.nlmsg_type = SOCK_DIAG_BY_FAMILY;
	Query.Header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	Query.Header.nlmsg_seq = 1;

	Query.Request.sdiag_family = (__u8)Family;
	Query.Request.sdiag_protocol = (__u8)Protocol;

	//
	// idiag_states is a bitmask of the TCP states to report. UDP sockets are
	// reported in TCP_CLOSE, so asking for everything covers both.
	//
	Query.Request.idiag_states = ~0u;

	//
	// Request the tcp_info payload; this is what carries the byte counters that
	// are the whole reason for preferring sock_diag over /proc/net.
	//
	Query.Request.idiag_ext = (1 << (INET_DIAG_INFO - 1));

	sockaddr_nl Address;
	memset(&Address, 0, sizeof(Address));
	Address.nl_family = AF_NETLINK;

	iovec Iov = { &Query, sizeof(Query) };
	msghdr Message;
	memset(&Message, 0, sizeof(Message));
	Message.msg_name = &Address;
	Message.msg_namelen = sizeof(Address);
	Message.msg_iov = &Iov;
	Message.msg_iovlen = 1;

	if (sendmsg(Socket, &Message, 0) < 0)
	{
		close(Socket);
		return false;
	}

	const bool bIPv6 = (Family == AF_INET6);
	const bool bTcp = (Protocol == IPPROTO_TCP);

	// 32 KiB is comfortably more than one netlink datagram; the kernel splits
	// the dump across as many as it needs.
	QByteArray Buffer(32 * 1024, '\0');
	bool bDone = false;
	bool bOk = true;

	while (!bDone)
	{
		const ssize_t Length = recv(Socket, Buffer.data(), Buffer.size(), 0);
		if (Length <= 0)
		{
			// EINTR aside, a failure here after a successful send means the
			// kernel refused the dump (no CONFIG_INET_DIAG, or a namespace
			// restriction).
			bOk = (errno == 0);
			break;
		}

		// NLMSG_NEXT decrements its length argument, so it needs a mutable copy.
		int Remaining = (int)Length;
		const nlmsghdr* pHeader = (const nlmsghdr*)Buffer.constData();
		for (; NLMSG_OK(pHeader, (unsigned)Remaining); pHeader = NLMSG_NEXT(pHeader, Remaining))
		{
			if (pHeader->nlmsg_type == NLMSG_DONE)
			{
				bDone = true;
				break;
			}
			if (pHeader->nlmsg_type == NLMSG_ERROR)
			{
				bOk = false;
				bDone = true;
				break;
			}

			const inet_diag_msg* pMsg = (const inet_diag_msg*)NLMSG_DATA(pHeader);

			ProcFs::SNetConnection Conn;
			Conn.ProtocolType = (bIPv6 ? NET_IPV6 : NET_IPV4) | (bTcp ? NET_TCP : NET_UDP);
			Conn.LocalAddress = ToAddress(pMsg->id.idiag_src, bIPv6);
			Conn.LocalPort = qFromBigEndian<quint16>(pMsg->id.idiag_sport);
			Conn.RemoteAddress = ToAddress(pMsg->id.idiag_dst, bIPv6);
			Conn.RemotePort = qFromBigEndian<quint16>(pMsg->id.idiag_dport);

			//
			// The state field only means anything for TCP. UDP reuses it, where
			// TCP_CLOSE is an ordinary unconnected socket - reporting that as
			// "Closed" would mislead, so UDP is pinned to ESTAB, which the
			// shared code renders as "Open".
			//
			Conn.State = bTcp ? TcpStateToMib(pMsg->idiag_state) : 5;

			Conn.Inode = pMsg->idiag_inode;
			Conn.Uid = pMsg->idiag_uid;
			Conn.TxQueue = pMsg->idiag_wqueue;
			Conn.RxQueue = pMsg->idiag_rqueue;

			//
			// Walk the attribute list for INET_DIAG_INFO. Only TCP sockets
			// carry a tcp_info; UDP has no equivalent, so UDP rates stay
			// unavailable rather than wrong.
			//
			int AttrLen = pHeader->nlmsg_len - NLMSG_LENGTH(sizeof(*pMsg));
			const rtattr* pAttr = (const rtattr*)(pMsg + 1);
			for (; RTA_OK(pAttr, AttrLen); pAttr = RTA_NEXT(pAttr, AttrLen))
			{
				if (pAttr->rta_type != INET_DIAG_INFO)
					continue;

				//
				// tcp_info grows between kernel versions, so the payload may be
				// shorter or longer than our struct. Copy only what both sides
				// have and check the field is actually covered before using it.
				//
				const unsigned PayloadLen = RTA_PAYLOAD(pAttr);
				tcp_info Info;
				memset(&Info, 0, sizeof(Info));
				memcpy(&Info, RTA_DATA(pAttr), qMin<unsigned>(PayloadLen, sizeof(Info)));

				const unsigned NeedForBytes = offsetof(tcp_info, tcpi_bytes_received) + sizeof(Info.tcpi_bytes_received);
				if (PayloadLen >= NeedForBytes)
				{
					// tcpi_bytes_acked counts payload bytes the peer has
					// acknowledged, which is the meaningful "sent" figure.
					Conn.BytesSent = Info.tcpi_bytes_acked;
					Conn.BytesReceived = Info.tcpi_bytes_received;
					Conn.bHaveCounters = true;
				}

				const unsigned NeedForSegs = offsetof(tcp_info, tcpi_segs_in) + sizeof(Info.tcpi_segs_in);
				if (PayloadLen >= NeedForSegs)
				{
					Conn.SegmentsSent = Info.tcpi_segs_out;
					Conn.SegmentsReceived = Info.tcpi_segs_in;
				}
				break;
			}

			Connections.append(Conn);
		}
	}

	close(Socket);
	return bOk;
}

bool IsAvailable()
{
	//
	// Probed once. A dump of TCP over IPv4 is the cheapest representative
	// request; if the kernel accepts that it will accept the rest.
	//
	static const bool bAvailable = []() -> bool {
		QList<ProcFs::SNetConnection> Probe;
		return DumpOne(AF_INET, IPPROTO_TCP, Probe);
	}();
	return bAvailable;
}

QList<ProcFs::SNetConnection> Enumerate()
{
	QList<ProcFs::SNetConnection> Connections;

	bool bAnyOk = false;
	bAnyOk |= DumpOne(AF_INET,  IPPROTO_TCP, Connections);
	bAnyOk |= DumpOne(AF_INET6, IPPROTO_TCP, Connections);
	bAnyOk |= DumpOne(AF_INET,  IPPROTO_UDP, Connections);
	bAnyOk |= DumpOne(AF_INET6, IPPROTO_UDP, Connections);

	// An outright failure returns empty so the caller falls back to /proc/net,
	// rather than silently reporting that the machine has no sockets.
	if (!bAnyOk)
		Connections.clear();

	return Connections;
}

} // namespace SockDiag
