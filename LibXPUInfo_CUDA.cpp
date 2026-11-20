// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/* To enable CUDA Driver API-based cudaOrdinal lookup,
   1. Run external\getCUDADep.bat to fetch cuda.h + cuda.lib (lightweight - no CUDA Toolkit install needed)
   2. Extract contents to $(SolutionDir)external\CUDA (done automatically by the script)
   3. Add XPUINFO_USE_CUDA to preprocessor definitions for LibXPUInfo project

   This uses only the CUDA Driver API (nvcuda.dll), which ships with the NVIDIA display
   driver itself - no CUDA Toolkit/Runtime install is required on end-user machines.
   The driver API's device enumeration order always matches NVML's (PCI bus order), unlike
   the CUDA Runtime (CUDART) which may reorder by FASTEST_FIRST heuristics. We match each
   driver-API device's PCI address against the corresponding XI::Device to assign a reliable
   cudaOrdinal.
*/
#ifdef XPUINFO_USE_CUDA
#include "LibXPUInfo.h"
#include "DebugStream.h"
#include "LibXPUInfo_Util.h"
#include "cuda.h"
#ifdef _WIN32
#pragma comment(lib, "cuda.lib")
#include <delayimp.h>
#endif

namespace XI
{

#ifdef _WIN32
static CUresult safeCuInit()
{
    CUresult result = CUDA_ERROR_NOT_INITIALIZED;
    __try
    {
        result = cuInit(0);
    }
    __except (
        ((GetExceptionCode() == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)) ||
         (GetExceptionCode() == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND)))
        ? EXCEPTION_EXECUTE_HANDLER
        : EXCEPTION_CONTINUE_SEARCH)
    {
        result = CUDA_ERROR_NOT_INITIALIZED;
    }
    return result;
}
#endif

void XPUInfo::initCUDA()
{
#ifdef _WIN32
    CUresult result = safeCuInit();
#else
    CUresult result = cuInit(0);
#endif

    constexpr bool printToConsole = false;
    DebugStream dStr(printToConsole);

    if (CUDA_SUCCESS != result)
    {
        dStr << "CUDA: Driver API init failed or not available" << std::endl;
        return;
    }
    dStr << "CUDA: Initialized" << std::endl;

    int deviceCount = 0;
    result = cuDeviceGetCount(&deviceCount);
    if (CUDA_SUCCESS != result)
    {
        dStr << "CUDA: Failed to query device count" << std::endl;
        return;
    }
    dStr << "CUDA:\tDevice count: " << deviceCount << std::endl;

    for (int ordinal = 0; ordinal < deviceCount; ++ordinal)
    {
        CUdevice cuDev = 0;
        result = cuDeviceGet(&cuDev, ordinal);
        if (CUDA_SUCCESS != result)
        {
            continue;
        }

        int pciDomainId = 0, pciBusId = 0, pciDeviceId = 0;
        CUresult rDomain = cuDeviceGetAttribute(&pciDomainId, CU_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID, cuDev);
        CUresult rBus = cuDeviceGetAttribute(&pciBusId, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID, cuDev);
        CUresult rDevice = cuDeviceGetAttribute(&pciDeviceId, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, cuDev);
        if ((CUDA_SUCCESS != rBus) || (CUDA_SUCCESS != rDevice))
        {
            continue;
        }

        PCIAddressType cudaPciAddr;
        cudaPciAddr.domain = (CUDA_SUCCESS == rDomain) ? (UI32)pciDomainId : 0;
        cudaPciAddr.bus = (UI32)pciBusId;
        cudaPciAddr.device = (UI32)pciDeviceId;
        cudaPciAddr.function = 0; // Driver API does not report function; nVidia GPUs are typically function 0

        dStr << "CUDA:\t\tDevice " << ordinal << ": " << cudaPciAddr.domain << ":" << cudaPciAddr.bus << ":" << cudaPciAddr.device << std::endl;

        for (auto& devPair : m_Devices)
        {
            auto& dev = devPair.second;
            if (dev->IsVendor(kVendorId_nVidia))
            {
                const auto& devPciAddr = dev->getProperties().PCIAddress;
                if ((devPciAddr.bus == cudaPciAddr.bus) &&
                    (devPciAddr.device == cudaPciAddr.device) &&
                    (devPciAddr.domain == cudaPciAddr.domain))
                {
                    updateIfDstNotSet(dev->m_props.VendorSpecific.nVidia.cudaOrdinal, ordinal);
                    m_UsedAPIs = m_UsedAPIs | API_TYPE_CUDA;
                    break;
                }
            }
        }
    }
}

} // namespace XI
#endif // XPUINFO_USE_CUDA
