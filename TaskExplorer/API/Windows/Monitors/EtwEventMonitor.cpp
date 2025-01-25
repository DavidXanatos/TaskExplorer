#include "stdafx.h"
#include "EtwEventMonitor.h"
#include "..\WindowsAPI.h"
#include ".\etw\krabs.hpp"

//#define USE_ETW_FILE_IO

#ifdef USE_ETW_FILE_IO
namespace krabs { namespace kernel {
CREATE_CONVENIENCE_KERNEL_PROVIDER(
	disk_file_io_provider ,
	EVENT_TRACE_FLAG_DISK_FILE_IO,
	krabs::guids::file_io);

CREATE_CONVENIENCE_KERNEL_PROVIDER(
	disk_file_io_provider_rd ,
	0,
	krabs::guids::file_io);
} }
#endif

struct SEtwEventMonitor
{
	SEtwEventMonitor(CEtwEventMonitor* This)
	 : kernel_trace(L"TaskExp_EtKernelLogger")
#ifdef USE_ETW_FILE_IO
	 , kernel_rundown(L"TaskExp_EtKernelRundown")
#endif
	 , user_trace(L"TaskExp_EtUserLogger")
		// A trace can have any number of providers, which are identified by GUID. These
		// GUIDs are defined by the components that emit events, and their GUIDs can
		// usually be found with various ETW tools (like wevutil).
	 , dns_res_provider(krabs::guid(L"{55404E71-4DB9-4DEB-A5F5-8F86E46DDE56}")) // Microsoft-Windows-Winsock-NameResolution
	{
		disk_provider.add_on_event_callback([This](const EVENT_RECORD &record) {
			//qDebug() << "disk event";
			krabs::schema schema(record);

			int Type = EventTypeUnknow;

			switch (schema.event_opcode())
			{
			case EVENT_TRACE_TYPE_IO_READ:
				Type = EtwDiskReadType;
				break;
			case EVENT_TRACE_TYPE_IO_WRITE:
				Type = EtwDiskWriteType;
				break;
			default:
				return;
			}

			krabs::parser parser(schema);

			quint64 ProcessId = -1;
			quint64 ThreadId = -1;

			// Since Windows 8, we no longer get the correct process/thread IDs in the
			// event headers for disk events. 
            //if (WindowsVersion >= WINDOWS_8)
			if(schema.thread_id() == ULONG_MAX)
            {
                ThreadId = parser.parse<uint32_t>(L"IssuingThreadId");
				ProcessId = -1; // indicate that it must be looked up by thread id
            }
            else if(schema.process_id() != ULONG_MAX)
            {
				ProcessId = schema.process_id();
				ThreadId = schema.thread_id();
            }

			quint64 FileObject = 0;
			krabs::binary FileObjectBin = parser.parse<krabs::binary>(L"FileObject");
			if (FileObjectBin.bytes().size() == 8)
				FileObject = *(uint64_t*)FileObjectBin.bytes().data();
			else if (FileObjectBin.bytes().size() == 4)
				FileObject = *(uint32_t*)FileObjectBin.bytes().data();
			quint32 IrpFlags = parser.parse<uint32_t>(L"IrpFlags");
			quint32 TransferSize = parser.parse<uint32_t>(L"TransferSize");
			quint64 HighResResponseTime = parser.parse<uint64_t>(L"HighResResponseTime");

			emit This->DiskEvent(Type, FileObject, ProcessId, ThreadId, IrpFlags, TransferSize, HighResResponseTime);
		});
		kernel_trace.enable(disk_provider);

#ifdef USE_ETW_FILE_IO
		auto file_callback = [](CEtwEventMonitor* This, const EVENT_RECORD &record) {
			//qDebug() << "file event";
			krabs::schema schema(record);
			
			qDebug() << QString("EventId: %1").arg(schema.event_id());
			qDebug() << QString("EventOpcode: %1").arg(schema.event_opcode());
			qDebug() << QString("ProcessId: %1").arg(schema.process_id());
			qDebug() << QString("ThreadId: %1").arg(schema.thread_id());
			qDebug() << QString("EventName: %1").arg(schema.event_name());
			qDebug() << QString("ProviderName: %1").arg(schema.provider_name());
	

			krabs::parser parser(schema);
			krabs::property_iterator PI = parser.properties();
			for (std::vector<krabs::property>::iterator I = PI.begin(); I != PI.end(); ++I)
			{
				QString Info = QString("\t %1:").arg(I->name());

				bool bOk = false;
			
				try { Info += QString::fromStdWString(parser.parse<std::wstring>(I->name()));	bOk = true; }
				catch (...) {}
			
				if(!bOk) try { Info += QString::number(parser.parse<uint64_t>(I->name()));	bOk = true; }
				catch (...) {}

				if (!bOk) try { Info += QString::number(parser.parse<uint32_t>(I->name()));	bOk = true; }
				catch (...) {}

				if (!bOk) try { Info += QString::number(parser.parse<ULONG_PTR>(I->name()));	bOk = true; }
				catch (...) {}

				if (!bOk) try { Info += QString::number((ULONG_PTR)parser.parse<void*>(I->name()));	bOk = true; }
				catch (...) {}


				if (!bOk) Info += QString("Type: %1").arg(I->type());

				qDebug() << Info;
			}

			qDebug() << "";

			int Type = EventTypeUnknow;

			switch (record.EventHeader.EventDescriptor.Opcode)
			{
			case 0: // Name
				Type = EtwFileNameType;
				break;
			case 32: // FileCreate
				Type = EtwFileCreateType;
				break;
			case 36: // FileRundown
				Type = EtwFileRundownType;
				break;
			case 35: // FileDelete
				Type = EtwFileDeleteType;
				break;
			}

			if (Type != EventTypeUnknow)
			{
				krabs::parser parser(schema);

				quint64 FileId = (quint64)parser.parse<void*>(L"FileObject");
				QString FileName = QString::fromStdWString(parser.parse<std::wstring>(L"FileName"));

				quint64 ProcessId = -1;
				quint64 ThreadId = -1;

				// Since Windows 8, we no longer get the correct process/thread IDs in the
				// event headers for file events. 
				//if (WindowsVersion >= WINDOWS_8)
				if (schema.process_id() != ULONG_MAX)
				{
					ProcessId = schema.process_id();
					ThreadId = schema.thread_id();
				}

				//EtDiskProcessFileEvent(&fileEvent);
				emit This->FileEvent(Type, FileId, ThreadId, ThreadId, FileName);
			}
		};

		file_provider.add_on_event_callback([This, file_callback](const EVENT_RECORD &record) { file_callback(This, record); });
		kernel_trace.enable(file_provider);

		file_provider_rd.add_on_event_callback([This, file_callback](const EVENT_RECORD &record) { file_callback(This, record); });
		kernel_rundown.enable(file_provider_rd);
#endif

		proc_provider.add_on_event_callback([This](const EVENT_RECORD &record) {
			//qDebug() << "process event";
			krabs::schema schema(record);

			if (schema.event_id() != 0)
				return;

			int Type = EventTypeUnknow;
			switch (schema.event_opcode())
			{
				case 1: Type = EtwProcessStarted; break;
				case 2: Type = EtwProcessStopped; break;
				default: // we dont care for other event types
					return;
			}

			krabs::parser parser(schema);

			quint32 ProcessId = parser.parse<uint32_t>(L"ProcessId");
			QString CommandLine = QString::fromStdWString(parser.parse<std::wstring>(L"CommandLine"));
			QString FileName = QString::fromStdString(parser.parse<std::string>(L"ImageFileName"));
			quint32 ParentId = parser.parse<uint32_t>(L"ParentId");

			//qDebug() << FILETIME2time(schema.timestamp().QuadPart) << GetTime();

			emit This->ProcessEvent(Type, ProcessId, CommandLine, FileName, ParentId, schema.timestamp().QuadPart);

		});
		kernel_trace.enable(proc_provider);


		auto net_callback = [](CEtwEventMonitor* This, const EVENT_RECORD &record) {
			//qDebug() << "net event";

			// TcpIp/UdpIp

			int Type = EventTypeUnknow;
			quint32 ProtocolType = 0;

			switch (record.EventHeader.EventDescriptor.Opcode)
			{
			case EVENT_TRACE_TYPE_SEND: // send
				Type = EtwNetworkSendType;
				ProtocolType = NET_TYPE_NETWORK_IPV4;
				break;
			case EVENT_TRACE_TYPE_RECEIVE: // receive
				Type = EtwNetworkReceiveType;
				ProtocolType = NET_TYPE_NETWORK_IPV4;
				break;
			case EVENT_TRACE_TYPE_SEND + 16: // send ipv6
				Type = EtwNetworkSendType;
				ProtocolType = NET_TYPE_NETWORK_IPV6;
				break;
			case EVENT_TRACE_TYPE_RECEIVE + 16: // receive ipv6
				Type = EtwNetworkReceiveType;
				ProtocolType = NET_TYPE_NETWORK_IPV6;
				break;
			default:
				return;
			}

			static GUID TcpIpGuid_I = { 0x9a280ac0, 0xc8e0, 0x11d1, { 0x84, 0xe2, 0x00, 0xc0, 0x4f, 0xb9, 0x98, 0xa2 } };
			static GUID UdpIpGuid_I = { 0xbf3a50c5, 0xa9c9, 0x4988, { 0xa0, 0x05, 0x2d, 0xf0, 0xb7, 0xc8, 0x0f, 0x80 } };

			if (IsEqualGUID(record.EventHeader.ProviderId, TcpIpGuid_I))
				ProtocolType |= NET_TYPE_PROTOCOL_TCP;
			else if (IsEqualGUID(record.EventHeader.ProviderId, UdpIpGuid_I))
				ProtocolType |= NET_TYPE_PROTOCOL_UDP;

			quint64 ProcessId = -1;
			quint32 TransferSize = 0;

			QHostAddress LocalAddress;
			quint16 LocalPort = 0;
			QHostAddress RemoteAddress;
			quint16 RemotePort = 0;

			if (ProtocolType & NET_TYPE_NETWORK_IPV4)
			{
				struct TcpIpOrUdpIp_IPV4_Header
				{
					ULONG PID;
					ULONG size;
					ULONG daddr;
					ULONG saddr;
					USHORT dport;
					USHORT sport;
					//UINT64 Aux;
				} *data = (TcpIpOrUdpIp_IPV4_Header*)record.UserData;

				ProcessId = data->PID;
				TransferSize = data->size;

				LocalAddress = QHostAddress(ntohl(data->saddr));
				LocalPort = ntohs(data->sport);

				RemoteAddress = QHostAddress(ntohl(data->daddr));
				RemotePort = ntohs(data->dport);
			}
			else if (ProtocolType & NET_TYPE_NETWORK_IPV6)
			{
				struct TcpIpOrUdpIp_IPV6_Header
				{
					ULONG PID;
					ULONG size;
					IN6_ADDR daddr;
					IN6_ADDR saddr;
					USHORT dport;
					USHORT sport;
					UINT64 Aux;
				} *data = (TcpIpOrUdpIp_IPV6_Header*)record.UserData;

				ProcessId = data->PID;
				TransferSize = data->size;

				LocalAddress = QHostAddress(data->saddr.u.Byte);
				LocalPort = ntohs(data->sport);

				RemoteAddress = QHostAddress(data->daddr.u.Byte);
				RemotePort = ntohs(data->dport);
			}

			// Note: Incomming UDP packets have the endpoints swaped :/
			if ((ProtocolType & NET_TYPE_PROTOCOL_UDP) != 0 && Type == EtwNetworkReceiveType)
			{
				QHostAddress TempAddresss = LocalAddress;
				quint16 TempPort = LocalPort;
				LocalAddress = RemoteAddress;
				LocalPort = RemotePort;
				RemoteAddress = TempAddresss;
				RemotePort = TempPort;
			}

			//if(ProcessId == )
			//	qDebug() << ProcessId  << Type << LocalAddress.toString() << LocalPort << RemoteAddress.toString() << RemotePort;

			//EtProcessNetworkEvent(&networkEvent);
			emit This->NetworkEvent(Type, ProcessId, -1, ProtocolType, TransferSize, LocalAddress, LocalPort, RemoteAddress, RemotePort);
		};

		tcp_provider.add_on_event_callback([This, net_callback](const EVENT_RECORD &record) { net_callback(This, record); });
		udp_provider.add_on_event_callback([This, net_callback](const EVENT_RECORD &record) { net_callback(This, record); });

		kernel_trace.enable(tcp_provider);
		kernel_trace.enable(udp_provider);


		// user_trace providers typically have any and all flags, whose meanings are
		// unique to the specific providers that are being invoked. To understand these
		// flags, you'll need to look to the ETW event producer.
		dns_res_provider.any(0xf0010000000003ff);
		dns_res_provider.add_on_event_callback([This](const EVENT_RECORD &record) {
			krabs::schema schema(record);

			if (schema.event_id() != 1001)
				return;

			krabs::parser parser(schema);

			uint32_t Status;
			if (!parser.try_parse(L"Status", Status) || Status != 0)
				return;

			QString HostName = QString::fromStdWString(parser.parse<std::wstring>(L"NodeName"));
			QString Result = QString::fromStdWString(parser.parse<std::wstring>(L"Result"));

			emit This->DnsResEvent(schema.process_id(), schema.thread_id(), HostName, Result.split(";", Qt::SkipEmptyParts));
		});
		user_trace.enable(dns_res_provider);


		kernel_thread = new std::thread([&]() { 
			kernel_trace.start();
		});

#ifdef USE_ETW_FILE_IO
		kernel_thread_rd = new std::thread([&]() { 
			kernel_rundown.start();
		});
#endif

		user_thread = new std::thread([&]() { 
			user_trace.start(); 
		});
	}

	~SEtwEventMonitor()
	{
		kernel_trace.stop();
		kernel_thread->join();
		delete kernel_thread;

#ifdef USE_ETW_FILE_IO
		kernel_rundown.stop();
		kernel_thread_rd->join();
		delete kernel_thread_rd;
#endif

		user_trace.stop();
		user_thread->join();
		delete user_thread;
	}

	krabs::kernel_trace kernel_trace;
	krabs::kernel::disk_io_provider disk_provider;
#ifdef USE_ETW_FILE_IO
	krabs::kernel::disk_file_io_provider file_provider;
#endif
	krabs::kernel::process_provider proc_provider;
	krabs::kernel::network_tcpip_provider tcp_provider;
	krabs::kernel::network_udpip_provider udp_provider;
	std::thread* kernel_thread = NULL;

#ifdef USE_ETW_FILE_IO
	krabs::kernel_trace kernel_rundown;
	krabs::kernel::disk_file_io_provider_rd file_provider_rd;
	std::thread* kernel_thread_rd = NULL;
#endif

	krabs::user_trace user_trace;
	krabs::provider<> dns_res_provider;
	std::thread* user_thread = NULL;
};

CEtwEventMonitor::CEtwEventMonitor(QObject *parent) : QObject(parent)
{
	m = NULL;
}

bool CEtwEventMonitor::Init()
{
	m = new SEtwEventMonitor(this);

	return true;
}

CEtwEventMonitor::~CEtwEventMonitor()
{
	delete m;
}

