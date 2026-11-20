// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_D3D12

#include "LibXPUInfo.h"
#include "DebugStream.h"
#include <d3d12.h>
using namespace winrt;

namespace XI
{

// Since this is currently used just for an optimization hint, avoid throwing on errors, just log
void Device::initD3D12Device(IUnknown* pAdapter)
{
    if (m_props.UMA & UMAType::CACHE_COHERENT_UMA_KNOWN)
    {
        // Only query so far - early-out if already done
        return;
    }
    constexpr bool printToConsole = 
#ifdef _DEBUG
        true
#else
        false
#endif
        ;
    DebugStream dStr(printToConsole);

    // d3d12.dll is loaded at runtime to avoid a link-time dependency; intentionally not freed.
    static HMODULE hD3D12 = LoadLibraryExW(L"d3d12.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    // Failures here should not prevent the rest of the system from working, so just return if we can't load the library.
    if (!hD3D12)
    {
        dStr << __FUNCTION__ << ": Failed to load d3d12.dll\n";
        return;
    }
    static auto pfnCreateDevice = reinterpret_cast<decltype(&D3D12CreateDevice)>(GetProcAddress(hD3D12, "D3D12CreateDevice"));
    if (!pfnCreateDevice)
    {
        dStr << __FUNCTION__ << ": Failed to get D3D12CreateDevice proc address\n";
        return;
    }

    com_ptr<ID3D12Device> d3d12Device;
    D3D_FEATURE_LEVEL minFeatureLevel = (getType() == DEVICE_TYPE_GPU) ? D3D_FEATURE_LEVEL_11_0 : D3D_FEATURE_LEVEL_1_0_CORE;
    if (SUCCEEDED(pfnCreateDevice(pAdapter, minFeatureLevel, IID_PPV_ARGS(d3d12Device.put()))))
    {
        D3D12_FEATURE_DATA_ARCHITECTURE arch{}; // NodeIndex 0
        if (SUCCEEDED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch))))
        {
            bool trustResult = ((m_props.UMA == UMAType::UMA_UNKNOWN) ||
                ((m_props.UMA != UMAType::UMA_INTEGRATED) ^ (!!arch.UMA)));
            if (!trustResult)
            {
                dStr << __FUNCTION__ << "UMA mismatch between DXCore and D3D12\n";
            }
            else
            {
                if (m_props.UMA == UMAType::UMA_UNKNOWN)
                {
                    m_props.UMA = arch.UMA ? UMAType::UMA_INTEGRATED : UMAType::NONUMA_DISCRETE;
                }
                m_props.UMA |= UMAType::CACHE_COHERENT_UMA_KNOWN;
                if (arch.CacheCoherentUMA)
                {
                    m_props.UMA |= UMAType::CACHE_COHERENT_UMA;
                }
                m_validAPIs |= API_TYPE_D3D12;
            }
        }
    }
}

} // namespace XI

#endif // XPUINFO_USE_D3D12
