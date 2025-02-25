// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/* To enable NVML,
   1. Get package from https://developer.download.nvidia.com/compute/cuda/redist/cuda_nvml_dev/windows-x86_64/, currently cuda_nvml_dev-windows-x86_64-12.5.39-archive.zip
   2. Extract contents to $(SolutionDir)external\NVML
   3. Add XPUINFO_USE_NVML to preprocessor definitions for LibXPUInfo project
   */
#ifdef XPUINFO_USE_NVML
#include "LibXPUInfo.h"
#include "DebugStream.h"
#include "LibXPUInfo_Util.h"
#include "nvml.h"
#pragma comment(lib, "nvml.lib")
#ifdef _WIN32
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

    // Ampere has 64 or 128 CUDA cores per SM
    UI32 numGPUCores = 0;
    result = nvmlDeviceGetNumGpuCores(device, &numGPUCores);
    if (NVML_SUCCESS == result)
    {
        // Overwrite OpenCL result which is SM, use CUDA cores
        m_props.NumComputeUnits = (I32)numGPUCores;
    }

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

    UI32 busWidth = 0;
    result = nvmlDeviceGetMemoryBusWidth(device, &busWidth);
    PRINT_IF_SUCCESS(busWidth, nvmlDeviceGetMemoryBusWidth);
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
            m_UsedAPIs = m_UsedAPIs | API_TYPE_NVML;

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

                        for (auto& [luid, dev] : m_Devices)
                        {
                            if (dev->getProperties().PCIAddress == pciAddr)
                            {
                                dev->initNVMLDevice(device);
                                break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            DebugStream dStr(true);
            dStr << "Failed to query device count: " << nvmlErrorString(result) << std::endl;
        }
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
