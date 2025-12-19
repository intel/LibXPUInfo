// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/* To enable NVML,
   1. Get package from https://developer.download.nvidia.com/compute/cuda/redist/cuda_nvml_dev/windows-x86_64/, currently cuda_nvml_dev-windows-x86_64-12.5.39-archive.zip
   2. Extract contents to $(SolutionDir)external\NVML
   3. Add XPUINFO_USE_NVML to preprocessor definitions for LibXPUInfo project
   */
#ifdef XPUINFO_USE_NVML
#include "LibXPUInfo.h"
#ifndef __linux__
#include "DebugStream.h"
#endif
#include "LibXPUInfo_Util.h"
#include "nvml.h"
#ifdef _WIN32
#pragma comment(lib, "nvml.lib")
#include <delayimp.h>
#endif

#if 0 // For experimenting with NVML
#define PRINT_IF_SUCCESS(var, funcName) if (NVML_SUCCESS==result) std::cout << #funcName << " -> " << #var << ": " << var << std::endl;
#else
#define PRINT_IF_SUCCESS(var, funcName)
#endif

namespace XI
{
void Device::initNVMLDevice(nvmlDevice_t device)
{
    nvmlReturn_t result;
    char name[NVML_DEVICE_NAME_BUFFER_SIZE];
    String nameStr;
    UI32 LinkGen = 0;
    UI32 LinkWidth = 0;

    result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
    if (NVML_SUCCESS == result)
    {
        nameStr = name;
    }

	#if NVML_API_VERSION>=12
    // Ampere has 64 or 128 CUDA cores per SM
    UI32 numGPUCores = 0;
    result = nvmlDeviceGetNumGpuCores(device, &numGPUCores);
    if (NVML_SUCCESS == result)
    {
        // Overwrite OpenCL result which is SM, use CUDA cores
        m_props.NumComputeUnits = (I32)numGPUCores;
    }
	#endif

    nvmlEnableState_t mode = NVML_FEATURE_DISABLED;
    result = nvmlDeviceGetPowerManagementMode(device, &mode);
    if (NVML_SUCCESS == result)
    {
    }
    UI32 powerLimit = 0;
    UI32 minPowerLimit = 0;
    result = nvmlDeviceGetPowerUsage(device, &powerLimit); // Seems to be current power draw
    PRINT_IF_SUCCESS(powerLimit, nvmlDeviceGetPowerUsage);
    result = nvmlDeviceGetPowerManagementDefaultLimit(device, &powerLimit); // dnw
    PRINT_IF_SUCCESS(powerLimit, nvmlDeviceGetPowerManagementDefaultLimit);
    result = nvmlDeviceGetPowerManagementLimit(device, &powerLimit); // dnw
    PRINT_IF_SUCCESS(powerLimit, nvmlDeviceGetPowerManagementLimit);
    result = nvmlDeviceGetPowerManagementLimitConstraints(device, &minPowerLimit, &powerLimit); // dnw
    PRINT_IF_SUCCESS(minPowerLimit, nvmlDeviceGetPowerManagementLimitConstraints);
    PRINT_IF_SUCCESS(powerLimit, nvmlDeviceGetPowerManagementLimitConstraints);
    result = nvmlDeviceGetEnforcedPowerLimit(device, &powerLimit); // dnw - should the the one we want
    PRINT_IF_SUCCESS(powerLimit, nvmlDeviceGetEnforcedPowerLimit);

    if (NVML_SUCCESS == result)
    {
        updateIfDstNotSet(m_props.PackageTDP, (I32)(powerLimit / 1000));
    }

#if NVML_API_VERSION>=12
    UI32 busWidth = 0;
    result = nvmlDeviceGetMemoryBusWidth(device, &busWidth);
    PRINT_IF_SUCCESS(busWidth, nvmlDeviceGetMemoryBusWidth);
#endif
    nvmlMemory_t memoryInfo;
    result = nvmlDeviceGetMemoryInfo(device, &memoryInfo);
    PRINT_IF_SUCCESS(memoryInfo.total, nvmlDeviceGetMemoryInfo);

    UI32 freqSM = 0, freqGfx = 0, freqMem=0;
    //result = nvmlDeviceGetClock(device, NVML_CLOCK_SM, NVML_CLOCK_ID_APP_CLOCK_DEFAULT, &freqSM);
    result = nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_SM, &freqSM);
    if (NVML_SUCCESS == result)
    {
        updateIfDstNotSet(m_props.FreqMaxMHz, (I32)freqSM);
    }
    PRINT_IF_SUCCESS(freqSM, nvmlDeviceGetMaxClockInfo);
    result = nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_GRAPHICS, &freqGfx);
    PRINT_IF_SUCCESS(freqGfx, nvmlDeviceGetMaxClockInfo);
    result = nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_MEM, &freqMem);
    PRINT_IF_SUCCESS(freqMem, nvmlDeviceGetMaxClockInfo);
    UI32 freqVideo = 0;
    result = nvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_VIDEO, &freqVideo);
    PRINT_IF_SUCCESS(freqVideo, nvmlDeviceGetMaxClockInfo);

    unsigned int clockMHz = 0;
    result = nvmlDeviceGetClock(device, NVML_CLOCK_GRAPHICS, NVML_CLOCK_ID_CURRENT, &clockMHz);
    PRINT_IF_SUCCESS(clockMHz, nvmlDeviceGetClock);

    nvmlUtilization_t utilization = {};
    result = nvmlDeviceGetUtilizationRates(device, &utilization);
    PRINT_IF_SUCCESS(utilization.gpu, nvmlDeviceGetClock);
    PRINT_IF_SUCCESS(utilization.memory, nvmlDeviceGetClock);

    result = nvmlDeviceGetCurrPcieLinkGeneration(device, &LinkGen);
    if (NVML_SUCCESS == result)
    {
        result = nvmlDeviceGetCurrPcieLinkWidth(device, &LinkWidth);
        if (NVML_SUCCESS == result)
        {
            updateIfDstNotSet(m_props.PCICurrentGen, (I32)LinkGen);
            updateIfDstNotSet(m_props.PCICurrentWidth, (I32)LinkWidth);
        }
    }

#if NVML_API_VERSION>=12
    nvmlDeviceArchitecture_t arch = 0;
    result = nvmlDeviceGetArchitecture(device, &arch);
    if (NVML_SUCCESS == result)
    {
        if (m_props.DeviceGenerationID < 0)
        {
            m_props.DeviceGenerationID = arch;
            m_props.DeviceGenerationAPI = API_TYPE_NVML;
        }
    }
#endif

    int cccMajor = 0, cccMinor = 0;
    result = nvmlDeviceGetCudaComputeCapability(device, &cccMajor, &cccMinor);
    if (NVML_SUCCESS == result)
    {
        m_props.VendorSpecific.nVidia.cudaComputeCapability_Major = cccMajor;
        m_props.VendorSpecific.nVidia.cudaComputeCapability_Minor = cccMinor;
    }

    // Currently only for multi-instance GPU (MIG)
#if 0
    nvmlDeviceAttributes_t devAttrs{};
    result = nvmlDeviceGetAttributes(device, &devAttrs);
    if (NVML_SUCCESS == result)
    {

    }
#endif

#if 0 // TODO: calculate MemoryBandWidthMax
    if (freqMem && busWidth)
    {
        I64 memBW_Bps = (busWidth / 8 * freqMem) * (1024*1024ULL);
        // TODO: Not correct - about half of actual
        //updateIfDstNotSet(m_props.MemoryBandWidthMax, memBW_Bps);
    }
#endif

    validAPIs = validAPIs | API_TYPE_NVML;
    m_nvmlDevice = device;
}

#ifdef XPUINFO_USE_TELEMETRYTRACKER
bool TelemetryTracker::RecordNVML(TimedRecord& rec)
{
    nvmlDevice_t dev = m_Device->getHandle_NVML();
    bool bRet = false;
    if (dev)
    {
        unsigned int clockMHz = 0;
        auto result = nvmlDeviceGetClock(dev, NVML_CLOCK_GRAPHICS, NVML_CLOCK_ID_CURRENT, &clockMHz);
        if (NVML_SUCCESS == result)
        {
            rec.freq = clockMHz;
            if (m_records.size() == 0)
            {
                m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_FREQUENCY);
            }
            bRet = true;
        }
        nvmlUtilization_t utilization = {};
        result = nvmlDeviceGetUtilizationRates(dev, &utilization);
        if (NVML_SUCCESS == result)
        {
            rec.activity_compute = utilization.gpu;
            rec.activity_global = utilization.memory;
            if (m_records.size() == 0)
            {
                m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_RENDER_COMPUTE_ACTIVITY| TELEMETRYITEM_GLOBAL_ACTIVITY);
            }
            bRet = true;
        }

    }
    return bRet;
}
#endif

#ifdef _WIN32
static nvmlReturn_t safeInitNVML()
{
    nvmlReturn_t result = NVML_ERROR_UNINITIALIZED;
    __try
    {
        result = nvmlInit();
    }
    __except (GetExceptionCode() == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)
        ? EXCEPTION_EXECUTE_HANDLER
        : EXCEPTION_CONTINUE_SEARCH)
    {
        result = NVML_ERROR_UNINITIALIZED;
    }
    return result;
}
#endif

void XPUInfo::initNVML()
{
#ifdef _WIN32
    nvmlReturn_t result = safeInitNVML();
#else
    nvmlReturn_t result = nvmlInit();
#endif

    if (NVML_SUCCESS == result)
    {
        UI32 device_count=0;
        result = nvmlDeviceGetCount(&device_count);
        if (NVML_SUCCESS == result)
        {
#ifndef __linux__
            m_UsedAPIs = m_UsedAPIs | API_TYPE_NVML;
#endif
            for (UI32 i = 0; i < device_count; ++i)
            {
                nvmlDevice_t device = nullptr;
                result = nvmlDeviceGetHandleByIndex(i, &device);
                if (NVML_SUCCESS == result)
                {
                    nvmlPciInfo_t pci;
                    result = nvmlDeviceGetPciInfo(device, &pci);
                    if (NVML_SUCCESS == result)
                    {
                        PCIAddressType pciAddr;
                        pciAddr.bus = pci.bus;
                        pciAddr.device = pci.device;
                        pciAddr.domain = pci.domain;
                        pciAddr.function = 0;
                        String dbdf = pci.busId;
                        auto posLastDot = dbdf.rfind('.');
                        String funcStr = dbdf.substr(posLastDot + 1);
                        if (funcStr[0] != '0')
                        {
                            pciAddr.function = atoi(funcStr.c_str());
                        }

#ifdef __linux__
// Add to devices
						DXGI_ADAPTER_DESC1 desc1{};
						desc1.VendorId = pci.pciDeviceId & 0xffff;
						desc1.DeviceId = pci.pciDeviceId >> 16;
						XPUINFO_REQUIRE(desc1.VendorId == kVendorId_nVidia);
						// desc1.SubSysId = hwID.subSysID;
						// desc1.Revision = hwID.revision;
						// desc1.DedicatedSystemMemory = DedicatedSystemMemory;
						nvmlReturn_t nvmlRet;
						
						nvmlMemory_t nvmlMem{};
						nvmlRet = nvmlDeviceGetMemoryInfo(device, &nvmlMem);
						if (NVML_SUCCESS == nvmlRet)
						{
							desc1.DedicatedVideoMemory = nvmlMem.total;
						}
						
						nvmlBAR1Memory_t bar1Mem{};
						nvmlRet = nvmlDeviceGetBAR1MemoryInfo(device, &bar1Mem);
						if (NVML_SUCCESS == nvmlRet)
						{
							desc1.SharedSystemMemory = bar1Mem.bar1Total;
						}
						
						#if NVML_API_VERSION>=12
						char uuid[NVML_DEVICE_UUID_V2_BUFFER_SIZE];
						nvmlRet = nvmlDeviceGetUUID(device, uuid, NVML_DEVICE_UUID_V2_BUFFER_SIZE);
						#else
						char uuid[NVML_DEVICE_UUID_BUFFER_SIZE];
						nvmlRet = nvmlDeviceGetUUID(device, uuid, NVML_DEVICE_UUID_BUFFER_SIZE);
						#endif
						if (NVML_SUCCESS == nvmlRet)
						{
							desc1.AdapterLuid = *(decltype(desc1.AdapterLuid)*)uuid;
						}
						
						char name[NVML_DEVICE_NAME_BUFFER_SIZE];
						WString nameStr;

						nvmlRet = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
						if (NVML_SUCCESS == nvmlRet)
						{
							nameStr = convert(name);
							std::wcscpy(desc1.Description, nameStr.c_str());
						}
						
						char version[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
						DeviceDriverVersion ddv(0);
						nvmlRet = nvmlSystemGetDriverVersion(version, NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE);
						if (NVML_SUCCESS == nvmlRet)
						{
							ddv = DeviceDriverVersion::FromString(version);
						}

						DevicePtr newDevice(new Device((UI32)m_Devices.size(), &desc1, 
							DEVICE_TYPE_GPU, API_TYPE_NVML, ddv.GetAsUI64()));

						auto newIt = m_Devices.find(newDevice->getLUID());
						if (newIt == m_Devices.end())
						{
							// Add
							auto insertResult = m_Devices.insert(std::make_pair(newDevice->getLUID(), newDevice));
							if (insertResult.second)
							{
								insertResult.first->second->initNVMLDevice(device);
								m_UsedAPIs = m_UsedAPIs | API_TYPE_NVML;
							}
						}
#else
                        for (auto& [luid, dev] : m_Devices)
                        {
                            if (dev->getProperties().PCIAddress == pciAddr)
                            {
                                dev->initNVMLDevice(device);
                                break;
                            }
                        }
#endif
                    }
                }
            }
        }
#ifndef __linux__
        else
        {
            DebugStream dStr(true);
            dStr << "Failed to query device count: " << nvmlErrorString(result) << std::endl;
        }
#endif
    }

}

void XPUInfo::shutdownNVML()
{
    if (m_UsedAPIs & API_TYPE_NVML)
    {
#ifdef _DEBUG
        auto result = 
#endif
            nvmlShutdown();
        XPUINFO_DEBUG_REQUIRE(NVML_SUCCESS == result);
    }
}

} // XI
#endif // XPUINFO_USE_LEVELZERO
