// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_D3D12

#include "LibXPUInfo.h"
#include <d3d12.h>
using namespace winrt;

namespace XI
{

void Device::initD3D12Device(IUnknown* pAdapter)
{
    if (m_props.UMA & UMAType::CACHE_COHERENT_UMA_KNOWN)
    {
        // Only query so far - early-out if already done
        return;
    }

    // d3d12.dll is loaded at runtime to avoid a link-time dependency; intentionally not freed.
    HMODULE hD3D12 = LoadLibraryExW(L"d3d12.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!hD3D12)
    {
        return;
    }
    auto pfnCreateDevice = reinterpret_cast<decltype(&D3D12CreateDevice)>(GetProcAddress(hD3D12, "D3D12CreateDevice"));
    if (!pfnCreateDevice)
    {
        return;
    }

    com_ptr<ID3D12Device> d3d12Device;
    if (SUCCEEDED(pfnCreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(d3d12Device.put()))))
    {
        D3D12_FEATURE_DATA_ARCHITECTURE arch{}; // NodeIndex 0
        if (SUCCEEDED(d3d12Device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch))))
        {
            XPUINFO_REQUIRE_MSG((m_props.UMA == UMAType::UMA_UNKNOWN) ||
                ((m_props.UMA != UMAType::UMA_INTEGRATED) ^ (!!arch.UMA)), "UMA mismatch between DXCore and D3D12");
            if (m_props.UMA == UMAType::UMA_UNKNOWN)
            {
                m_props.UMA = arch.UMA ? UMAType::UMA_INTEGRATED : UMAType::NONUMA_DISCRETE;
            }
            m_props.UMA |= UMAType::CACHE_COHERENT_UMA_KNOWN;
            if (arch.CacheCoherentUMA)
            {
                m_props.UMA |= UMAType::CACHE_COHERENT_UMA;
            }
            m_validAPIs = m_validAPIs | API_TYPE_D3D12;
        }
    }
}

} // namespace XI

#endif // XPUINFO_USE_D3D12
