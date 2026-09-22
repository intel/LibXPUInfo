#pragma once
// Copyright (C) 2026 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <string>
#include <memory>
#include <cstdint>

// ** Fwd Decl **
// Level Zero
typedef struct _ze_driver_handle_t* ze_driver_handle_t;
typedef struct _ze_device_handle_t* ze_device_handle_t;
typedef struct _zes_freq_handle_t* zes_freq_handle_t;
typedef struct _zes_engine_handle_t* zes_engine_handle_t;
typedef struct _ze_device_properties_t ze_device_properties_t;
typedef struct _ze_driver_extension_properties_t ze_driver_extension_properties_t;

// OpenCL
typedef struct _cl_platform_id* cl_platform_id;
typedef struct _cl_device_id* cl_device_id;

#if defined(_WIN32) || defined(__linux__)
// NVML
typedef struct nvmlDevice_st* nvmlDevice_t;
#endif

// AGS
typedef struct AGSGPUInfo AGSGPUInfo;

#ifdef _WIN32
// SetupAPI
typedef PVOID HDEVINFO;
#else
// Windows types used for cross-OS compatibility
typedef union {std::uint64_t ui64;} LUID; // Union primarily to make it a different type than XI::UI64 for overloading
typedef std::uint32_t UINT;
typedef std::uint32_t DWORD;
typedef size_t SIZE_T;
typedef wchar_t WCHAR;
typedef struct DXGI_ADAPTER_DESC1
{
    WCHAR Description[ 128 ];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;
    UINT Flags;
}   DXGI_ADAPTER_DESC1;

struct DXCoreAdapterMemoryBudget
{
    std::uint64_t budget;
    std::uint64_t currentUsage;
    std::uint64_t availableForReservation;
    std::uint64_t currentReservation;
};
#endif

namespace XI
{
    using String = std::string;
    using WString = std::wstring;
    using UI64 = std::uint64_t;
    using UI32 = std::uint32_t;
    using UI16 = std::uint16_t;
    using U8 = unsigned char;
    using I64 = std::int64_t;
    using I32 = std::int32_t;
    using I16 = std::int16_t;
    using I8 = char;
    class SystemInfo; // Fwd decl
    class L0_Extensions; // Fwd decl

    template <typename T>
    using SharedPtr = std::shared_ptr<T>;

    struct XPUINFO_EXPORT NoCopyAssign
    {
        NoCopyAssign() {};
        NoCopyAssign(const NoCopyAssign&) = delete;
        NoCopyAssign& operator=(const NoCopyAssign&) = delete;
    };

    enum DeviceType : UI32
    {
        DEVICE_TYPE_UNKNOWN = 0,
        DEVICE_TYPE_CPU     = 1,
        DEVICE_TYPE_GPU     = 1 << 1,
        DEVICE_TYPE_NPU     = 1 << 2,
        DEVICE_TYPE_OTHER   = 1 << 3,
    };
    std::ostream& operator<<(std::ostream& s, DeviceType t);

    enum APIType : UI32
    {
        API_TYPE_UNKNOWN =                  0,
        API_TYPE_DXGI =                     1,
        API_TYPE_DX11_INTEL_PERF_COUNTER =  1 << 1,
        API_TYPE_OPENCL =                   1 << 3,
        API_TYPE_LEVELZERO =                1 << 4,
        API_TYPE_SETUPAPI =                 1 << 5,
        API_TYPE_DXCORE =                   1 << 6,
        API_TYPE_NVML =                     1 << 7,
        API_TYPE_METAL =                    1 << 8,
        API_TYPE_WMI =                      1 << 9,
        API_TYPE_DESERIALIZED =             1 << 10,
        API_TYPE_AGS =                      1 << 12,
        API_TYPE_LAST =                     1 << 13,

        // Removed:
        //API_TYPE_IGCL =                     1 << 2,
        //API_TYPE_IGCL_L0 =                  1 << 11, // Allow IGCL to use L0.  Once L0 issue with ZE_INIT_FLAG_VPU_ONLY is resolved, this can be removed.
    };
    inline APIType operator|=(APIType& a, APIType b) {
        a = static_cast<APIType>(a | b);
        return a;
    }

#ifdef _WIN32
    // WMI takes more time to initialize than others, 
    // so it is not included in this default all-API macro
    // If WMI is desired, use APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI)
#ifndef _M_ARM64
#define XPUINFO_INIT_ALL_APIS (XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI \
    | XI::API_TYPE_DX11_INTEL_PERF_COUNTER | XI::API_TYPE_OPENCL \
    | XI::API_TYPE_LEVELZERO \
    | XI::API_TYPE_DXCORE | XI::API_TYPE_NVML | XI::API_TYPE_AGS)
#else
#define XPUINFO_INIT_ALL_APIS (XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI \
    | XI::API_TYPE_DXCORE | XI::API_TYPE_NVML)
#endif
#elif defined(__linux__)
#define XPUINFO_INIT_ALL_APIS XI::API_TYPE_NVML
#else
#define XPUINFO_INIT_ALL_APIS XI::API_TYPE_METAL
#endif

    inline APIType operator|(APIType l, APIType r)
    {
        return static_cast<APIType>(static_cast<UI32>(l) | static_cast<UI32>(r));
    }
    XPUINFO_EXPORT std::ostream& operator<<(std::ostream& s, APIType t);

    enum UMAType : UI32
    {
        UMA_UNKNOWN =       0,
        UMA_INTEGRATED =    1,
        NONUMA_DISCRETE =   1 << 1
    };

    template <typename T>
    inline double BtoGB(T n)
    {
        return (double(n) / (1024.0 * 1024 * 1024));
    }
    template <typename T>
    inline double BtoKB(T n)
    {
        return (double(n) / 1024.0);
    }

    enum class IntelGfxArchitecture : UI32
    {
        iUnknown = 0,
        iGen9 = 9,
        iGen11 = 11,
        iGen12orXe = 12,
        iXe2 = 20,
        iXe3 = 30,
        iXe3p = 35,
    };

    // See https://github.com/uxlfoundation/oneDNN/blob/f0af9656d9bf4e286f44e14ef8e152c066e328c4/third_party/ngen/ngen_core.hpp#L268
    enum class IntelGfxFamily : UI32
    {
        iUnknown,
        iGen9_Generic,
        iGen11_Generic,
        iGen12LP_Generic,
        iGen12HP_DG2,
        iXe_S, // MTL-U, ARL-S, ARL-U
        iXe_L_MeteorLakeH,
        iXe_L_ArrowLakeH,
        iXe2_Generic,
        iXe2_LunarLake,
        iXe2_BattleMage,
        iXe3_Generic,
        iXe3_PantherLake,
        iXe3_NovaLake,
        iXe3p_Generic,
        iXe3p_NovaLakeP,
    };
    typedef std::pair<IntelGfxFamily, std::string> IntelGfxFamilyNamePair;

    enum class IntelNPUArchitecture : UI32
    {
        Unknown,
        NPU_2_7, // Meteor Lake, Arrow Lake
        NPU_4,   // Lunar Lake
        NPU_5,   // Panther Lake
        NPU_6,   // Nova Lake
    };

    constexpr UINT kVendorId_Intel = 0x8086;
    constexpr UINT kVendorId_nVidia = 0x10de;
    constexpr UINT kVendorId_AMD = 0x1002;
    constexpr UINT kVendorId_Qualcomm = 'Q' | ('C' << 8) | ('O' << 16) | ('M' << 24);
} // namespace XI
