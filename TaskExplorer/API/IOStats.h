#pragma once 
#include <qobject.h>
#include "../Common/Common.h"

#define AVG_INTERVAL (3*1000)

struct SRateCounter
{
	SRateCounter()
	{
		ByteRate = 0;
		TotalBytes = 0;
		TotalTime = 0;
	}

	virtual void Update(quint64 Interval, quint64 AddDelta)
	{
		// ToDo; instead of pushing and popping use a sircualr buffer approche.

		while(TotalTime > AVG_INTERVAL && !RateStat.empty())
		{
			TotalTime -= RateStat.front().Interval;
			TotalBytes -= RateStat.front().Bytes;
			RateStat.pop_front();
		}

		RateStat.push_back(SStat(Interval, AddDelta));
		TotalTime += RateStat.back().Interval;
		TotalBytes += RateStat.back().Bytes;

		quint64 totalTime = TotalTime ? TotalTime : Interval;
		if(totalTime < AVG_INTERVAL/2)
			totalTime = AVG_INTERVAL;
		ByteRate = TotalBytes*1000/totalTime;
		ASSERT(ByteRate >= 0);
	}

	__inline quint64	Get() const			{return ByteRate;}

protected:
	quint64				ByteRate;

	struct SStat
	{
		SStat(quint64 i, quint64 b) {Interval = i; Bytes = b;}
		quint64 Interval;
		quint64 Bytes;
	};
	list<SStat>			RateStat;
	quint64				TotalBytes;
	quint64				TotalTime;
};

struct SRateCounter2 : SRateCounter
{
	SRateCounter2()
	{
		TotalRate = 0;
	}

	virtual void Update(quint64 Interval, quint64 AddDelta)
	{
		SRateCounter::Update(Interval, AddDelta);

		while(Smooth.size() > 4)
		{
			TotalRate -= Smooth.front();
			Smooth.pop_front();
		}
		Smooth.push_back(ByteRate);
		TotalRate += ByteRate;

		ByteRate = TotalRate / Smooth.size();
	}

	list<quint64>		Smooth;
	quint64				TotalRate;
};

struct SNetStats
{
	SNetStats()
	{
		ReceiveCount = 0;
		ReceiveRaw = 0;
		SendCount = 0;
		SendRaw = 0;			
	}

	void AddReceive(quint64 size) {
		ReceiveCount++;
		ReceiveRaw += size;
	}

	void AddSend(quint64 size) {
		SendCount++;
		SendRaw += size;
	}

	void UpdateStats(quint64 Interval)
	{
		ReceiveDelta.Update(ReceiveCount);
		ReceiveRawDelta.Update(ReceiveRaw);
		ReceiveRate.Update(Interval, ReceiveRawDelta.Delta);
		SendDelta.Update(SendCount);
		SendRawDelta.Update(SendRaw);
		SendRate.Update(Interval, SendRawDelta.Delta);
	}

	quint64			ReceiveCount;
	quint64			ReceiveRaw;
	quint64			SendCount;
	quint64			SendRaw;

    SDelta64		ReceiveDelta;
    SDelta64		ReceiveRawDelta;
    SDelta64		SendDelta;
    SDelta64		SendRawDelta;

	SRateCounter2	ReceiveRate;
	SRateCounter2	SendRate;
};

struct SIOStats
{
	SIOStats()
	{
		ReadCount = 0;
		ReadRaw = 0;
		WriteCount = 0;	
		WriteRaw = 0;
	}

	void SetRead(quint64 size, quint64 count) {
		ReadCount = count;
		ReadRaw = size;
	}

	void SetWrite(quint64 size, quint64 count) {
		WriteCount = count;
		WriteRaw = size;
	}

	void UpdateStats(quint64 Interval)
	{
		ReadDelta.Update(ReadCount);
		ReadRawDelta.Update(ReadRaw);
		ReadRate.Update(Interval, ReadRawDelta.Delta);
		WriteDelta.Update(WriteCount);
		WriteRawDelta.Update(WriteRaw);
		WriteRate.Update(Interval, WriteRawDelta.Delta);
	}

	quint64			ReadCount;
	quint64			ReadRaw;
	quint64			WriteCount;
	quint64			WriteRaw;

	SDelta64		ReadDelta;
    SDelta64		ReadRawDelta;
    SDelta64		WriteDelta;
    SDelta64		WriteRawDelta;

	SRateCounter	ReadRate;
	SRateCounter	WriteRate;
};

struct SDiskStats: SIOStats
{
	SDiskStats()
	{
	}

	void AddRead(quint64 size) {
		ReadCount++;
		ReadRaw += size;
	}

	void AddWrite(quint64 size) {
		WriteCount++;
		WriteRaw += size;
	}
};

struct SIOStatsEx: SDiskStats
{
	SIOStatsEx()
	{
		OtherCount = 0;	
		OtherRaw = 0;
	}

	void SetOther(quint64 size, quint64 count) {
		OtherCount = count;
		OtherRaw = size;
	}


	void UpdateStats(quint64 Interval)
	{
		SDiskStats::UpdateStats(Interval);
		OtherDelta.Update(OtherCount);
		OtherRawDelta.Update(OtherRaw);
		OtherRate.Update(Interval, OtherRawDelta.Delta);
	}

	quint64			OtherCount;
	quint64			OtherRaw;

    SDelta64		OtherDelta;
    SDelta64		OtherRawDelta;

	SRateCounter	OtherRate;
};

struct SSockStats
{
	quint64 UpdateStats()
	{
		quint64 curTick = GetCurTick();
		quint64 time_ms = curTick - LastStatUpdate;
		LastStatUpdate = curTick;

		Net.UpdateStats(time_ms);

		return time_ms;
	}

	quint64		LastStatUpdate;

	SNetStats	Net;
};

struct SProcStats
{
	SProcStats()
	{
		LastStatUpdate = 0;
	}

	quint64 UpdateStats()
	{
		quint64 curTick = GetCurTick();
		quint64 time_ms = curTick - LastStatUpdate;
		LastStatUpdate = curTick;

		Net.UpdateStats(time_ms);
		Disk.UpdateStats(time_ms);
		Io.UpdateStats(time_ms);

		return time_ms;
	}

	quint64		LastStatUpdate;

	SNetStats	Net;
	SDiskStats	Disk;
	SIOStatsEx	Io;
};

struct SSysStats: SProcStats
{
	quint64 UpdateStats()
	{
		quint64 time_ms = SProcStats::UpdateStats();

		MMapIo.UpdateStats(time_ms);

		NetIf.UpdateStats(time_ms);
		//NetVpn.UpdateStats(time_ms);
		NetRas.UpdateStats(time_ms);

#ifdef WIN32
		SambaServer.UpdateStats(time_ms);
		SambaClient.UpdateStats(time_ms);
#endif

		return time_ms;
	}

	SIOStatsEx	MMapIo;

	SNetStats	NetIf;
	//SNetStats	NetVpn;
	SNetStats	NetRas;

#ifdef WIN32
	SNetStats	SambaServer;
	SNetStats	SambaClient;
#endif
};


struct SUnOverflow
{
	SUnOverflow()
	{
		OverflowCount = 0;
		Value = 0;
	}

	quint64	FixValue(quint32 curValue)
	{
		if (curValue < Value)
			OverflowCount++;
		Value = curValue;
		return GetValue();
	}

	__inline quint64 GetValue()
	{
		return ((quint64)OverflowCount * 0xFFFFFFFFui64) + (quint64)Value;
	}


	quint32	OverflowCount;
	quint32 Value;
};