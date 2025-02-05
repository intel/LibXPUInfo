// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_TELEMETRYTRACKER
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#ifdef _WIN32
#include <Psapi.h>
#include <Pdh.h>
#include <PdhMsg.h>
#pragma comment(lib, "pdh")
#endif // _WIN32

#include <iomanip>

namespace XI
{

void TelemetryTracker::printRecordHeader(std::ostream& ostr) const
{
	ostr << "Time(s)";
#if defined(_WIN32) && !defined(_M_ARM64)
	ostr << ", %CPU, CPU Freq (MHz), GPU Local Mem Used (GB), GPU Shared Mem (GB), GPU Dedicated Mem (GB), GPU Total Mem (GB)";
#endif
	if (m_ResultMask & TELEMETRYITEM_FREQUENCY)
		ostr << ", Freq(MHz)";
	bool haveBW = (m_ResultMask & (TELEMETRYITEM_READ_BW | TELEMETRYITEM_WRITE_BW)) == (TELEMETRYITEM_READ_BW | TELEMETRYITEM_WRITE_BW);
	if (haveBW)
	{
		ostr << ",Rd BW(MB/s),Wr BW(MB/s),BW(MB/s)";
	}
	if (m_ResultMask & TELEMETRYITEM_GLOBAL_ACTIVITY)
		ostr << ",% Global";
	if (m_ResultMask & TELEMETRYITEM_RENDER_COMPUTE_ACTIVITY)
		ostr << ",% Compute";
	if (m_ResultMask & TELEMETRYITEM_MEDIA_ACTIVITY)
		ostr << ",% Media";
	if (m_ResultMask & TELEMETRYITEM_MEMORY_USAGE)
		ostr << ",Device Memory Used (MB)";
	if (m_ResultMask & TELEMETRYITEM_FREQUENCY_MEDIA)
		ostr << ",Media Freq (MHz)";
	if (m_ResultMask & TELEMETRYITEM_FREQUENCY_MEMORY)
		ostr << ",Memory Freq (GT/s)";
	if (m_ResultMask & TELEMETRYITEM_SYSTEMMEMORY)
		ostr << ",Physical System Memory Available (GB),Commit Total (GB),Commit Limit (GB),Commit Peak (GB)";

	ostr << std::endl;
}

UI64 TelemetryTracker::getMaxMemUsage() const 
{
	UI64 maxMemUsage = 0;
	for (const auto& rec : m_records)
	{
		maxMemUsage = std::max(maxMemUsage, rec.deviceMemoryUsedBytes);
	}
	return maxMemUsage;
}

UI64 TelemetryTracker::getInitialMemUsage() const
{
	UI64 memUsage = 0;
	if (m_records.size())
	{
		memUsage = m_records.front().deviceMemoryUsedBytes;
	}
	return memUsage;
}

const DevicePtr& TelemetryTracker::getDevice() const
{
    return m_Device;
}

void TelemetryTracker::printRecord(TimedRecords::const_iterator it, std::ostream& ostr) const
{
	const auto& rec = *it;
	const auto default_precision{ ostr.precision() };

	if (m_ResultMask & TELEMETRYITEM_TIMESTAMP_DOUBLE)
	{
		ostr << rec.timeStamp - m_startTime;
	}
	else
	{
		XPUINFO_REQUIRE(m_timestamp_freq != 0);
		auto elapsed_Secs = (rec.timeStampUI64 - m_startTimeUI64) / double(m_timestamp_freq);
		ostr << elapsed_Secs;
	}

#if defined(_WIN32) && !defined(_M_ARM64)
	ostr << "," << rec.pctCPU << "," << rec.cpu_freq / (100.0);
	ostr << "," << rec.gpu_mem_Local / (1024.0 * 1024 * 1024);
	ostr << "," << rec.gpu_mem_Adapter_Shared / (1024.0 * 1024 * 1024);
	ostr << "," << rec.gpu_mem_Adapter_Dedicated / (1024.0 * 1024 * 1024);
	ostr << "," << rec.gpu_mem_Adapter_Total / (1024.0 * 1024 * 1024);
#endif

	if (m_ResultMask & TELEMETRYITEM_FREQUENCY)
	{
		ostr << "," << rec.freq;
	}
	bool haveBW = (m_ResultMask & (TELEMETRYITEM_READ_BW | TELEMETRYITEM_WRITE_BW)) == (TELEMETRYITEM_READ_BW | TELEMETRYITEM_WRITE_BW);
	if ((it - m_records.begin()) > 0)
	{
		const auto& prev = *(it - 1);
		double tDelta = rec.timeStamp - prev.timeStamp;

		if (haveBW)
		{
			UI64 rdDelta = rec.bw_read - prev.bw_read;
			UI64 wrDelta = rec.bw_write - prev.bw_write;
			UI64 bwDelta = rdDelta + wrDelta;
			ostr << "," << rdDelta / (tDelta * (1024 * 1024));
			ostr << "," << wrDelta / (tDelta * (1024 * 1024));
			ostr << "," << bwDelta / (tDelta * (1024 * 1024));
		}
		if ((m_ResultMask & (TELEMETRYITEM_GLOBAL_ACTIVITY | TELEMETRYITEM_TIMESTAMP_DOUBLE)) == (TELEMETRYITEM_GLOBAL_ACTIVITY | TELEMETRYITEM_TIMESTAMP_DOUBLE))
		{
			ostr << "," << (rec.activity_global - prev.activity_global) * 100. / tDelta;
		}
		if ((m_ResultMask & (TELEMETRYITEM_RENDER_COMPUTE_ACTIVITY | TELEMETRYITEM_TIMESTAMP_DOUBLE)) == (TELEMETRYITEM_RENDER_COMPUTE_ACTIVITY | TELEMETRYITEM_TIMESTAMP_DOUBLE))
		{
			ostr << "," << (rec.activity_compute - prev.activity_compute) * 100. / tDelta;
		}
		if ((m_ResultMask & (TELEMETRYITEM_MEDIA_ACTIVITY | TELEMETRYITEM_TIMESTAMP_DOUBLE)) == (TELEMETRYITEM_MEDIA_ACTIVITY | TELEMETRYITEM_TIMESTAMP_DOUBLE))
		{
			ostr << "," << (rec.activity_media - prev.activity_media) * 100. / tDelta;
		}
	}
	else
	{
		if (haveBW)
		{
			ostr << ",,,";
		}
	}
	if (m_ResultMask & TELEMETRYITEM_GLOBAL_ACTIVITY)
	{
		if (!(m_ResultMask & TELEMETRYITEM_TIMESTAMP_DOUBLE))
		{
			ostr << "," << rec.activity_global;
		}
		else if ((it - m_records.begin()) == 0)
		{
			ostr << ",";
		}
	}
	if (m_ResultMask & TELEMETRYITEM_RENDER_COMPUTE_ACTIVITY)
	{
		if (!(m_ResultMask & TELEMETRYITEM_TIMESTAMP_DOUBLE))
		{
			ostr << "," << rec.activity_compute;
		}
		else if ((it - m_records.begin()) == 0)
		{
			ostr << ",";
		}
	}
	if (m_ResultMask & TELEMETRYITEM_MEDIA_ACTIVITY)
	{
		if ((it - m_records.begin()) == 0)
		{
			ostr << ",";
		}
	}
	if (m_ResultMask & TELEMETRYITEM_MEMORY_USAGE)
	{
		//ostr << "," << 100.0 * rec.deviceMemoryUsedBytes / (rec.deviceMemoryBudgetBytes);
		ostr << "," << (rec.deviceMemoryUsedBytes) / (1024.0 * 1024);
	}

	if (m_ResultMask & TELEMETRYITEM_FREQUENCY_MEDIA)
	{
		ostr << "," << (rec.freq_media);
	}
	if (m_ResultMask & TELEMETRYITEM_FREQUENCY_MEMORY)
	{
		ostr << "," << std::setprecision(3) << (rec.freq_memory / 1000.0) << std::setprecision(default_precision);
	}
	if (m_ResultMask & TELEMETRYITEM_SYSTEMMEMORY)
	{
        SaveRestoreIOSFlags saveFlags(ostr);
		ostr << "," << std::setprecision(5) << (rec.systemMemoryPhysicalAvailable / (1024.0 * 1024 * 1024))
			<< "," << (rec.systemMemoryCommitTotal / (1024.0 * 1024 * 1024))
			<< "," << (rec.systemMemoryCommitLimit / (1024.0 * 1024 * 1024))
			<< "," << (rec.systemMemoryCommitPeak / (1024.0 * 1024 * 1024))
			;
	}

	ostr << std::endl;
}

String TelemetryTracker::getLog() const
{
	std::ostringstream ostr;
	ostr << "Stats for " << convert(m_Device->name()) << " (" << m_msPeriod << "ms interval):" << std::endl;
	printRecordHeader(ostr);
	if (m_records.size())
	{
		for (TimedRecords::const_iterator it = m_records.begin(); it != m_records.end(); ++it)
		{
			printRecord(it, ostr);
		}
	}
	else
	{
		ostr << "TelemetryTracker: No records!\n";
	}

	return ostr.str();
}

TelemetryTracker::TelemetryTracker(const DevicePtr& deviceToTrack, UI32 msPeriod, std::ostream* pRealTimeOutputStream):
	m_Device(deviceToTrack), m_msPeriod(msPeriod),
	m_ResultMask(TELEMETRYITEM_UNKNOWN),
	m_pRealtime_ostr(pRealTimeOutputStream)
{
#if defined(_WIN32) && !defined(_M_ARM64)
	InitPDH();

	if (m_Device->getCurrentAPIs() & (API_TYPE_IGCL|API_TYPE_LEVELZERO|API_TYPE_DXCORE))
	{
		m_records.reserve(1024);

		InitializeThreadpoolEnvironment(&m_CallBackEnviron);

		// Create a cleanup group for this thread pool.
		m_cleanupgroup = CreateThreadpoolCleanupGroup();

		// Create a timer with the same callback environment.
		m_timer = CreateThreadpoolTimer(MyTimerCallback,
			this,
			&m_CallBackEnviron);

		if (nullptr == m_timer) {
			printf("CreateThreadpoolTimer failed. LastError: %lu\n",
				GetLastError());
		}
	}

	if ((m_Device->getCurrentAPIs() & API_TYPE_IGCL) == 0)
	{
		BOOL bRet = QueryPerformanceFrequency((LARGE_INTEGER*)&m_timestamp_freq);
		XPUINFO_REQUIRE(bRet);
	}

#ifdef XPUINFO_USE_IGCL
    if ((m_Device->getCurrentAPIs() & (API_TYPE_IGCL|API_TYPE_IGCL_L0)) == (API_TYPE_IGCL|API_TYPE_IGCL_L0))
	{
		InitIGCL();
	}
#endif

#endif // Win32

#ifdef XPUINFO_USE_LEVELZERO
	if (m_Device->getCurrentAPIs() & API_TYPE_LEVELZERO)
	{
		InitL0();
	}
#endif
}

TelemetryTracker::~TelemetryTracker() noexcept(false)
{
	stop();
	//
	// Wait for all callbacks to finish.
	// CloseThreadpoolCleanupGroupMembers also releases objects
	// that are members of the cleanup group, so it is not necessary 
	// to call close functions on individual objects 
	// after calling CloseThreadpoolCleanupGroupMembers.
	//
#if defined(_WIN32) && !defined(_M_ARM64)
	if (m_pdhQuery)
	{
		PDH_STATUS pdhs = PdhCloseQuery(m_pdhQuery);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);
	}

	if (m_cleanupgroup)
	{
		CloseThreadpoolCleanupGroupMembers(m_cleanupgroup,
			FALSE,
			nullptr);
		// Clean up the cleanup group.
		CloseThreadpoolCleanupGroup(m_cleanupgroup);
	}
#endif
    if (m_pRealtime_ostr) {
        (*m_pRealtime_ostr) << std::flush;
    }
}

void TelemetryTracker::start()
{
#if defined(_WIN32) && !defined(_M_ARM64)
	if (m_timer && m_msPeriod)
	{
		// Set the timer to fire now
		FILETIME FileDueTime = { 0 };

		SetThreadpoolTimer(m_timer,
			&FileDueTime,
			m_msPeriod,
			0);
	}
#endif
}

void TelemetryTracker::stop()
{
#if defined(_WIN32) && !defined(_M_ARM64)
	if (m_timer)
	{
		SetThreadpoolTimer(m_timer,
			nullptr,
			0,
			0);
	}
#endif
}

bool TelemetryTracker::RecordCPUTimestamp(TimedRecord& rec)
{
    // See std::chrono
#if defined(_WIN32) && !defined(_M_ARM64)
	BOOL bRet = QueryPerformanceCounter((LARGE_INTEGER*)&rec.timeStampUI64);
	XPUINFO_REQUIRE(bRet);
#else
    bool bRet = false;
#endif

	if (m_records.size() == 0)
	{
		m_startTimeUI64 = rec.timeStampUI64;
	}

	return !!bRet;
}

bool TelemetryTracker::RecordMemoryUsage(TimedRecord& rec)
{
#ifdef _WIN32
	// Note: If we want process-specific info, see these APIs:
	//  GetProcessMemoryInfo /PROCESS_MEMORY_COUNTERS_EX2 -- See https://learn.microsoft.com/en-us/windows/win32/psapi/enumerating-all-processes?redirectedfrom=MSDN
	//  GlobalMemoryStatusEx

	PERFORMANCE_INFORMATION pi;
	if (GetPerformanceInfo(&pi, sizeof(pi)))
	{
		rec.systemMemoryPhysicalAvailable = pi.PhysicalAvailable * pi.PageSize;
		rec.systemMemoryCommitTotal = pi.CommitTotal * pi.PageSize;
		rec.systemMemoryCommitLimit = pi.CommitLimit * pi.PageSize;
		rec.systemMemoryCommitPeak = pi.CommitPeak * pi.PageSize;

		if (!(m_ResultMask & TELEMETRYITEM_SYSTEMMEMORY))
		{
			m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_SYSTEMMEMORY);
		}
	}
#endif

	auto memusage = m_Device->getMemUsage();
	if (memusage.currentUsage)
	{
		rec.deviceMemoryUsedBytes = memusage.currentUsage;
		rec.deviceMemoryBudgetBytes = memusage.budget;
		if (!(m_ResultMask & TELEMETRYITEM_MEMORY_USAGE))
		{
			m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_MEMORY_USAGE);
		}
		return true;
	}
	return false;
}

void TelemetryTracker::RecordNow()
{
	TimedRecord rec{};
	bool bUpdate = false;
	std::lock_guard<std::mutex> lock(m_RecordMutex); // for now, only support one at a time

	// Frequency, throttleReason (L0, IGCL)
	// Memory (VRAM) read/write/timestamp (IGCL)
	// Engine Utilization (IGCL, PDH/perfmon)
	// PCIe bandwidth (IGCL, L0 zes_pci_state_t, zes_pci_stats_t)

	// IGCL:
	// * Freq
	// * VRAM Read BW
	// * VRAM Write BW

	if (m_Device->getCurrentAPIs() & API_TYPE_DXCORE)
	{
		bUpdate = RecordMemoryUsage(rec) || bUpdate;
	}

#ifdef XPUINFO_USE_IGCL
	if (m_Device->getCurrentAPIs() & API_TYPE_IGCL)
	{
		bUpdate = RecordIGCL(rec) || bUpdate;
	}
#endif // XPUINFO_USE_IGCL

#if defined(_WIN32) && !defined(_M_ARM64)
	bUpdate = RecordCPU_PDH(rec) || bUpdate;
#endif

#ifdef XPUINFO_USE_LEVELZERO
	if (m_Device->getCurrentAPIs() & API_TYPE_LEVELZERO)
	{
		bool bL0 = RecordL0(rec);
		bUpdate = bL0 || bUpdate;
	}
#endif

#ifdef XPUINFO_USE_NVML
	if (m_Device->getCurrentAPIs() & API_TYPE_NVML)
	{
		bUpdate = RecordNVML(rec) || bUpdate;
	}
#endif

	if (bUpdate)
	{
		if (!(m_Device->getCurrentAPIs() & API_TYPE_IGCL)) // Need CPU timestamp
		{
			RecordCPUTimestamp(rec);
		}

		m_records.push_back(rec);
		if (m_pRealtime_ostr)
		{
			if (m_records.size() == 1)
			{
				printRecordHeader(*m_pRealtime_ostr);
			}
			printRecord(m_records.end()-1, *m_pRealtime_ostr);
		}
	}
}

#if defined(_WIN32) && !defined(_M_ARM64)
static std::string getLuidStringForPDH(const LUID& luid)
{
	std::ostringstream luidStream;
	luidStream << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << luid.HighPart <<
		"_0x" << std::setw(8) << std::setfill('0') << luid.LowPart;
	return luidStream.str();
}

void TelemetryTracker::InitPDH()
{
	static const char counterCPU_T[] = "\\Processor(_Total)\\% Processor Time";
	/* Processor Utility is the amount of work a processor is completing,
	   as a percentage of the amount of work the processor could complete
	   if it were running at its nominal performanceand never idle.
	   On some processors, Processor Utility may exceed 100 % .
	*/
	static const char counterCPU_Utility[] = "\\Processor Information(_Total)\\% Processor Utility"; // Matches Win10 task manager CPU %Utilization
	static const char counterCPU_Freq[] = "\\Processor Information(_Total)\\Processor Frequency";
	static const char counterCPU_PctPerf[] = "\\Processor Information(_Total)\\% Processor Performance";
	//"\\GPU Adapter Memory(luid_0x00000000_0x00013E1A_phys_0)\Total Committed"
	static const char* counterGPU_Memory = "\\GPU Adapter Memory(luid_0x";
	static const char* counterGPU_Memory_Shared = "*)\\Shared Usage";
	static const char* counterGPU_Memory_Dedicated = "*)\\Dedicated Usage";
	static const char* counterGPU_Memory_Total = "*)\\Total Committed";
	static const char* counterGPULocal_Memory[] = { "\\GPU Local Adapter Memory(luid_0x", "*)\\Local Usage" };

	std::string luidPdhStr(getLuidStringForPDH(getDevice()->getLUIDAsStruct()));
	std::string gpuMemoryLocalCounterPath = counterGPULocal_Memory[0] + luidPdhStr + counterGPULocal_Memory[1];
	std::string gpuMemSharedCounterPath = counterGPU_Memory + luidPdhStr + counterGPU_Memory_Shared;
	std::string gpuMemDedicatedCounterPath = counterGPU_Memory + luidPdhStr + counterGPU_Memory_Dedicated;
	std::string gpuMemTotalCounterPath = counterGPU_Memory + luidPdhStr + counterGPU_Memory_Total;

	PDH_STATUS pdhs = PdhOpenQueryA(nullptr, 0, &m_pdhQuery);
	if (pdhs == ERROR_SUCCESS)
	{
		// static syntax: \\Computer\PerfObject(ParentInstance/ObjectInstance#InstanceIndex)\Counter
		pdhs = PdhAddCounterA(m_pdhQuery, counterCPU_Utility, 0, &m_pdhCtrCPU);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);

		pdhs = PdhAddCounterA(m_pdhQuery, counterCPU_Freq, 0, &m_pdhCtrCPUFreq);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);

		pdhs = PdhAddCounterA(m_pdhQuery, counterCPU_PctPerf, 0, &m_pdhCtrCPUPctPerf);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);
		
		pdhs = PdhAddCounterA(m_pdhQuery, gpuMemoryLocalCounterPath.c_str(), 0, &m_pdhCtrGPUMemLocal);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);

		pdhs = PdhAddCounterA(m_pdhQuery, gpuMemSharedCounterPath.c_str(), 0, &m_pdhCtrGPUAdapterMemShared);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);

		pdhs = PdhAddCounterA(m_pdhQuery, gpuMemDedicatedCounterPath.c_str(), 0, &m_pdhCtrGPUAdapterMemDedicated);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);

		pdhs = PdhAddCounterA(m_pdhQuery, gpuMemTotalCounterPath.c_str(), 0, &m_pdhCtrGPUAdapterMemTotal);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);

		// The first query does not return valid data, so do it here.
		pdhs = PdhCollectQueryData(m_pdhQuery);
		XPUINFO_REQUIRE(ERROR_SUCCESS == pdhs);
	}
}

bool TelemetryTracker::RecordCPU_PDH(TimedRecord& rec)
{
	PDH_STATUS pdhs = PdhCollectQueryData(m_pdhQuery); // Collects the current raw data value for all counters in the specified query and updates the status code of each counter.
	if (ERROR_SUCCESS == pdhs)
	{
		PDH_FMT_COUNTERVALUE DisplayValue;
		DWORD CounterType;

		pdhs = PdhGetFormattedCounterValue(m_pdhCtrCPU,
			PDH_FMT_DOUBLE,
			&CounterType,
			&DisplayValue);
		if (ERROR_SUCCESS != pdhs)
		{
			return false;
		}
		rec.pctCPU = DisplayValue.doubleValue;

		// This freq is constant, multiply by %Perf to match task manager
		pdhs = PdhGetFormattedCounterValue(m_pdhCtrCPUFreq,
			PDH_FMT_DOUBLE,
			&CounterType,
			&DisplayValue);
		if (ERROR_SUCCESS != pdhs)
		{
			return false;
		}
		rec.cpu_freq = DisplayValue.doubleValue;

		pdhs = PdhGetFormattedCounterValue(m_pdhCtrCPUPctPerf,
			PDH_FMT_DOUBLE,
			&CounterType,
			&DisplayValue);
		if (ERROR_SUCCESS != pdhs)
		{
			return false;
		}
		rec.cpu_freq *= DisplayValue.doubleValue;

		pdhs = PdhGetFormattedCounterValue(m_pdhCtrGPUMemLocal,
			PDH_FMT_DOUBLE,
			&CounterType,
			&DisplayValue);
		if (ERROR_SUCCESS != pdhs)
		{
			return false;
		}
		rec.gpu_mem_Local = DisplayValue.doubleValue;

		pdhs = PdhGetFormattedCounterValue(m_pdhCtrGPUAdapterMemTotal,
			PDH_FMT_DOUBLE,
			&CounterType,
			&DisplayValue);
		if (ERROR_SUCCESS != pdhs)
		{
			return false;
		}
		rec.gpu_mem_Adapter_Total = DisplayValue.doubleValue;

		pdhs = PdhGetFormattedCounterValue(m_pdhCtrGPUAdapterMemShared,
			PDH_FMT_DOUBLE,
			&CounterType,
			&DisplayValue);
		if (ERROR_SUCCESS != pdhs)
		{
			return false;
		}
		rec.gpu_mem_Adapter_Shared = DisplayValue.doubleValue;

		pdhs = PdhGetFormattedCounterValue(m_pdhCtrGPUAdapterMemDedicated,
			PDH_FMT_DOUBLE,
			&CounterType,
			&DisplayValue);
		if (ERROR_SUCCESS != pdhs)
		{
			return false;
		}
		rec.gpu_mem_Adapter_Dedicated = DisplayValue.doubleValue;

		return true;
	}
	return false;
}
#endif

} // XI
#endif // XPUINFO_USE_TELEMETRYTRACKER
