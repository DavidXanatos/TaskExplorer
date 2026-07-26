#include "stdafx.h"
#include "UdevMonitor.h"

#include <QSocketNotifier>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <linux/netlink.h>

CUdevMonitor::CUdevMonitor(QObject* parent)
	: QObject(parent)
{
	m_Socket = -1;
	m_pNotifier = nullptr;
}

CUdevMonitor::~CUdevMonitor()
{
	if (m_pNotifier)
	{
		m_pNotifier->setEnabled(false);
		delete m_pNotifier;
		m_pNotifier = nullptr;
	}

	if (m_Socket != -1)
	{
		close(m_Socket);
		m_Socket = -1;
	}
}

bool CUdevMonitor::Init()
{
	if (m_Socket != -1)
		return true;

	const int Socket = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, NETLINK_KOBJECT_UEVENT);
	if (Socket < 0)
		return false;

	//
	// A hotplug burst can outrun us; a larger receive buffer makes a dropped
	// event less likely. Failure is not fatal - the default size still works.
	//
	const int BufferSize = 256 * 1024;
	setsockopt(Socket, SOL_SOCKET, SO_RCVBUF, &BufferSize, sizeof(BufferSize));

	sockaddr_nl Address;
	memset(&Address, 0, sizeof(Address));
	Address.nl_family = AF_NETLINK;
	// nl_pid 0 lets the kernel assign one, so several instances can coexist.
	Address.nl_pid = 0;

	bool bBound = false;
	for (unsigned Group : { 1u, 2u })
	{
		Address.nl_groups = Group;
		if (bind(Socket, (sockaddr*)&Address, sizeof(Address)) == 0)
		{
			bBound = true;
			break;
		}
	}

	if (!bBound)
	{
		close(Socket);
		return false;
	}

	m_Socket = Socket;

	m_pNotifier = new QSocketNotifier(m_Socket, QSocketNotifier::Read, this);
	connect(m_pNotifier, SIGNAL(activated(QSocketDescriptor)), this, SLOT(OnReadyRead()));

	return true;
}

void CUdevMonitor::OnReadyRead()
{
	bool bRelevant = false;

	//
	// Drain everything queued. A single plug event produces one uevent per
	// device node involved, so this commonly reads several at once - hence one
	// signal at the end rather than one per message.
	//
	char Buffer[8192];
	for (;;)
	{
		const ssize_t Length = recv(m_Socket, Buffer, sizeof(Buffer), 0);
		if (Length <= 0)
			break; // EAGAIN: nothing left

		//
		// The payload is NUL-separated KEY=VALUE. Only the subsystem matters
		// here, and it is matched against the three the monitors enumerate.
		// Anything else - input devices, USB interfaces, power supplies - would
		// only cause pointless re-enumeration.
		//
		const char* pEnd = Buffer + Length;
		for (const char* p = Buffer; p < pEnd; p += strnlen(p, pEnd - p) + 1)
		{
			if (strncmp(p, "SUBSYSTEM=", 10) != 0)
				continue;

			const char* pSubsystem = p + 10;
			if (strcmp(pSubsystem, "block") == 0 ||
			    strcmp(pSubsystem, "net") == 0 ||
			    strcmp(pSubsystem, "drm") == 0)
			{
				bRelevant = true;
			}
			break; // one SUBSYSTEM per message
		}
	}

	if (bRelevant)
		emit HardwareChanged();
}
