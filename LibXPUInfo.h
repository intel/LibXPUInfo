// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Description: 
//  The XPUInfo API is intended to coalesce data about devices 
//  (CPU, GPU, and accelerators like Intel NPU) from both standard Windows 
//  APIs and vendor-specific APIs, providing an easy mechanism for 
//  applications to gain an understanding of the hardware at runtime.
//
//  The TelemetryTracker class provides realtime status/performance data.
//
//  Modifies and uses HybridDetect.h from https://github.com/GameTechDev/HybridDetect

/* These macros need to be set the same for both LibXPUInfo and clients:
*  XPUINFO_USE_RAPIDJSON, XPUINFO_USE_RUNTIMEVERSIONINFO,
*  XPUINFO_USE_SYSTEMEMORYINFO, 
*  
*  These do not have a direct impact on behavior of class LibXPUInfo, but should still be set the same:
*  XPUINFO_USE_TELEMETRYTRACKER, XPUINFO_USE_IPC
*/

#pragma once
#ifdef _WIN32
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS // for wcsncpy, strncpy
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif
#include <dxgi1_4.h>
#endif // _WIN32
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <mutex>
#include <limits>
#include <functional>
#include <sstream>

namespace XI {
    typedef void(*ErrorHandlerType)(const std::string& message, const char* fileName, const int lineNumber);
    ErrorHandlerType getErrorHandlerFunc();
    ErrorHandlerType setErrorHandlerFunc(ErrorHandlerType f);
}
#define ENABLE_PER_LOGICAL_CPUID_ISA_DETECTION 0
#define XPUINFO_REQUIRE(x) if (!(x)) XI::getErrorHandlerFunc()(#x, __FILE__, __LINE__)
#define XPUINFO_REQUIRE_MSG(x, msg) if (!(x)) { \
            std::ostringstream ostr; \
            ostr << msg; \
            XI::getErrorHandlerFunc()(ostr.str(), __FILE__, __LINE__); \
        }

#ifdef _DEBUG
#define XPUINFO_DEBUG_REQUIRE(x) if (!(x)) XI::getErrorHandlerFunc()(#x, __FILE__, __LINE__)
#else
#define XPUINFO_DEBUG_REQUIRE(x)
#endif
#define HYBRIDDETECT_DEBUG_REQUIRE(x) XPUINFO_DEBUG_REQUIRE(x)
#include "HybridDetect/HybridDetect.h"
#define XPUINFO_CPU_X86_64 HYBRIDDETECT_CPU_X86_64

#if defined(__INTEL_LLVM_COMPILER)
#ifndef _SILENCE_CLANG_COROUTINE_MESSAGE
#define _SILENCE_CLANG_COROUTINE_MESSAGE // silence coroutine not supported with clang error
#endif
#endif
#ifdef _WIN32
#include <dxcore.h>
#include <winrt/base.h> // For DXCore COM usage
typedef HANDLE       PDH_HCOUNTER;
typedef HANDLE       PDH_HQUERY;
#endif

#ifdef XPUINFO_USE_RAPIDJSON
#include "rapidjson/document.h"
#endif

/*
A list GPU, their type (i.e. discrete, integrated), model number, name, memory, and optional addition information like cores,...  
If this list then the only plausiable way to run the ML pipeline will OpenVINO-CPU, ORT-CPU.
Given a GPU entry from this list, it will tell us <hardware, framework> to run this on.  
    Hardware: CPU, iGPU, DiscreteGPU, CPU.  
    Framework is OpenVINO, OpenVINO-CPU, ORT-CPU, TensorRT.
*/

// TODO: For nVidia GPU info, find path to driver files, run nvidia-smi - see "nvidia-smi --help-query-gpu"
// i.e. nvidia-smi --query-gpu=timestamp,name,pci.bus_id,driver_version,pstate,pcie.link.gen.max,pcie.link.gen.current,pcie.link.width.current,temperature.gpu,utilization.gpu,utilization.memory,memory.total,memory.free,memory.used,power.default_limit,power.max_limit,enforced.power.limit,power.limit,clocks.max.gr --format=csv
// For direct-access, see NVML at https://developer.nvidia.com/nvidia-management-library-nvml
// TODO: For AMD, see https://gpuopen.com/gpuperfapi/, see ADLX
// 

// ** Fwd Decl **
// IGCL
typedef struct _ctl_device_adapter_handle_t* ctl_device_adapter_handle_t;
typedef struct _ctl_freq_handle_t* ctl_freq_handle_t;

// Level Zero
typedef struct _ze_driver_handle_t* ze_driver_handle_t;
typedef struct _ze_device_handle_t* ze_device_handle_t;
typedef struct _zes_freq_handle_t* zes_freq_handle_t;
typedef struct _ze_device_properties_t ze_device_properties_t;
typedef struct _ze_driver_extension_properties_t ze_driver_extension_properties_t;

// OpenCL
typedef struct _cl_platform_id* cl_platform_id;
typedef struct _cl_device_id* cl_device_id;

#ifdef _WIN32
// SetupAPI
typedef PVOID HDEVINFO;
// NVML
typedef struct nvmlDevice_st* nvmlDevice_t;
#else
// Windows types used for cross-OS compatibility
typedef union {std::uint64_t ui64;} LUID; // Union primarily to make it a different type than XI::UI64 for overloading
typedef std::uint32_t UINT;
typedef std::uint32_t DWORD;
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
    uint64_t budget;
    uint64_t currentUsage;
    uint64_t availableForReservation;
    uint64_t currentReservation;
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

    struct NoCopyAssign
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
        API_TYPE_IGCL =                     1 << 2,
        API_TYPE_OPENCL =                   1 << 3,
        API_TYPE_LEVELZERO =                1 << 4,
        API_TYPE_SETUPAPI =                 1 << 5,
        API_TYPE_DXCORE =                   1 << 6,
        API_TYPE_NVML =                     1 << 7,
        API_TYPE_METAL =                    1 << 8,
        API_TYPE_WMI =                      1 << 9,
        API_TYPE_DESERIALIZED =             1 << 10,
        API_TYPE_IGCL_L0 =                  1 << 11, // Allow IGCL to use L0.  Once L0 issue with ZE_INIT_FLAG_VPU_ONLY is resolved, this can be removed.
        API_TYPE_LAST =                     1 << 12,
    };
    inline APIType operator|=(APIType& a, APIType b) {
        a = static_cast<APIType>(a | b);
        return a;
    }

#ifdef _WIN32
    // WMI takes more time to initialize than others, 
    // so it is not included in this default all-API macro
    // If WMI is desired, use APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI)
#define XPUINFO_INIT_ALL_APIS (XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI \
    | XI::API_TYPE_DX11_INTEL_PERF_COUNTER | XI::API_TYPE_IGCL | XI::API_TYPE_OPENCL \
    | XI::API_TYPE_LEVELZERO \
    | XI::API_TYPE_DXCORE | XI::API_TYPE_NVML)
#else
#define XPUINFO_INIT_ALL_APIS XI::API_TYPE_METAL
#endif

    inline APIType operator|(APIType l, APIType r)
    {
        return static_cast<APIType>(static_cast<UI32>(l) | static_cast<UI32>(r));
    }
    std::ostream& operator<<(std::ostream& s, APIType t);

    enum UMAType : UI32
    {
        UMA_UNKNOWN =       0,
        UMA_INTEGRATED =    1,
        NONUMA_DISCRETE =   1 << 1
    };

    inline double BtoGB(size_t n)
    {
        return (n / (1024.0 * 1024 * 1024));
    }
    inline double BtoKB(size_t n)
    {
        return (n / 1024.0);
    }

    struct IGCLAdapterProperties;
    typedef SharedPtr<IGCLAdapterProperties> IGCLAdapterPropertiesPtr;
    struct IGCLPciProperties;
    typedef SharedPtr<IGCLPciProperties> IGCLPciPropertiesPtr;

    // Helper class to get driver version with given adapter luid.
    class DeviceDriverVersion
    {
    public:
        using VersionRange = std::pair<DeviceDriverVersion, DeviceDriverVersion>;
        DeviceDriverVersion(LUID inLuid);
        DeviceDriverVersion(uint64_t inRawVersion) : mRawVersion(inRawVersion), mValid(true) {};
        static DeviceDriverVersion FromString(const std::string& version);
        static const DeviceDriverVersion& GetMax();
        static const DeviceDriverVersion& GetMin();
        String GetAsString() const;
        WString GetAsWString() const;
        std::uint64_t GetAsUI64() const { return mRawVersion; }
        static const std::uint16_t kReleaseNumber_Ignore = 0xffff;
        bool AtLeast(std::uint16_t inBuildNumberLast4Digits, std::uint16_t inRelease = kReleaseNumber_Ignore) const;
        bool Valid() const { return mValid; }
        bool InRange(const VersionRange& range) const; // return (*this >= range.first) && (*this <= range.second)

    protected:
        bool CompareGE(const DeviceDriverVersion& rhs) const; // return (*this >= rhs)
        std::uint64_t mRawVersion;
        bool mValid = false;
    };

	struct L0Enum
	{
		L0Enum() : driver(nullptr) {};
		ze_driver_handle_t driver;
		std::vector<ze_device_handle_t> devices;
	};

    inline UI64 LuidToUI64(void* pluid)
    {
        return *(reinterpret_cast<UI64*>(pluid));
    }

    inline UI64 LuidToUI64(const LUID& luid)
    {
        return LuidToUI64(const_cast<LUID*>(&luid));
    }

    struct ResizableBARStatus
    {
        bool valid = false;
        bool supported = false;
        bool enabled = false;
    };

    struct PCIAddressType
    {
        UI32 domain;
        UI32 bus;
        UI32 device;
        UI32 function;
        PCIAddressType(UI32 dom=-1, UI32 b=-1, UI32 dev=-1, UI32 f=-1):
            domain(dom), bus(b), device(dev), function(f) {}
        bool valid() const;
        bool GetFromWStr(const WString& inStr);
#ifdef XPUINFO_USE_RAPIDJSON
        template <typename rjDocOrVal>
        PCIAddressType(const rjDocOrVal& val); // deserialize
        template <typename Alloc>
        rapidjson::Value serialize(Alloc& a) const;
#endif
        bool operator==(const PCIAddressType& inRHS) const;
    };

    struct DriverInfo
    {
        LUID DeviceLUID = {};
        WString DriverDesc;
        WString DeviceDesc;
        WString DriverVersion;
        WString DriverInfSection; // DEVPKEY_Device_DriverInfSection
        WString DeviceInstanceId; // DEVPKEY_Device_InstanceId, to correlate with WMI data
        PCIAddressType LocationInfo;
#ifdef _WIN32
        FILETIME DriverDate = {};   // When driver was created/published
        FILETIME InstallDate = {};  // When driver was installed
        static float SystemTimeToYears(const SYSTEMTIME& inSysTime);
        static float DriverAgeInYears(const FILETIME& inFileTime, SYSTEMTIME& outSysTime);
#endif
        float DriverAgeInYears() const;
    };
    typedef std::shared_ptr<DriverInfo> DriverInfoPtr;

    const UINT kVendorId_Intel = 0x8086;
    const UINT kVendorId_nVidia = 0x10de;

    // Properties that are frequently used or common to most devices
    struct DeviceProperties
    {
        DXGI_ADAPTER_DESC1 dxgiDesc;
        DriverInfoPtr pDriverInfo;

        // Memory
        UI64 DedicatedMemorySize = UI64(-1);
        UI64 SharedMemorySize = UI64(-1);
        I64 MemoryBandWidthMax = -1; // bytes/sec

        // PCIe
        I32 PCIDeviceGen = -1;
        I32 PCIDeviceWidth = -1;
        I32 PCICurrentGen = -1;
        I32 PCICurrentWidth = -1;
        I64 PCIDeviceMaxBandwidth = -1;
        I64 PCICurrentMaxBandwidth = -1;
        ResizableBARStatus PCIReBAR; // TODO: Add L0 zes_pci_bar_properties_1_2_t
        PCIAddressType PCIAddress;

        // Implementation characteristics
        UMAType UMA = UMA_UNKNOWN;
        I32 FreqMaxMHz = -1;
        I32 FreqMinMHz = -1;
        I32 DeviceGenerationID = -1;
        UI32 DeviceIPVersion = 0; // Vendor-specific, multi-API device generation. 0 is unknown
        APIType DeviceGenerationAPI = API_TYPE_UNKNOWN;
        const char* getDeviceGenerationName() const;
        I32 NumComputeUnits = -1;
        I32 ComputeUnitSIMDWidth = -1;
        I32 PackageTDP = -1; // Watts, Thermal Design Power.

        I32 MediaFreqMaxMHz = -1;
        I32 MediaFreqMinMHz = -1;
        I32 MemoryFreqMaxMHz = -1;
        I32 MemoryFreqMinMHz = -1;

        // TDP: ctl_power_properties_t, ctl_power_peak_limit_t
        // 
        
        // ctl_mem_properties_t
        // - type (GDDR6...), bus width, channels
        union
        {
            struct
            {
                U8 FLAG_DP4A : 1;
                U8 FLAG_DPAS : 1;
            } IntelFeatureFlags;
            UI32 IntelFeatureFlagsUI32;
        } VendorFlags = {};         // From OpenCL

        I8 IsHighPerformance = -1;  // Only 1 Device will be set.  From DXCore.
        I8 IsMinimumPower = -1;     // Only 1 Device will be set.  From DXCore.
        I8 IsDetachable = -1;       // From DXCore

        // From WMI
        String VideoMode;
        I32 RefreshRate = -1;

        // Dedicated + Shared, for comparing static device characteristics
        // For runtime decisions, see Device::getMemUsage()
        UI64 getTotalVideoMemorySize() const { return dxgiDesc.DedicatedVideoMemory + dxgiDesc.SharedSystemMemory; }
        UI64 getVideoMemorySize() const;
        bool operator==(const DeviceProperties& props) const;
    };

    class DeviceBase
    {
    public:
        DeviceBase(UI32 inIndex, DeviceType inType=DEVICE_TYPE_UNKNOWN) : m_adapterIndex(inIndex), m_type(inType) {}
        virtual ~DeviceBase() {}
        DeviceType getType() const { return m_type; }
        UI32 getAdapterIndex() const { return m_adapterIndex; }

        virtual WString name() const = 0;
        virtual UI64 getLUID() const = 0;

        static const UI32 kAdapterIndex_CPU = UI32(-1);
        static const UI64 kLUID_CPU = UI64(-1LL);

    protected:
        UI32 m_adapterIndex; // DXGI EnumAdapters1 order or -1 for CPU
        DeviceType m_type;
    };

    class DeviceCPU : public DeviceBase
    {
    public:
        DeviceCPU();
        virtual ~DeviceCPU() {}
        virtual WString name() const override;
        virtual UI64 getLUID() const override { return kLUID_CPU; };

        void printInfo(std::ostream& ostr, const SystemInfo* pSysInfo=nullptr) const;
        const HybridDetect::PROCESSOR_INFO* getProcInfo() const { return m_pProcInfo.get(); }

        // Use to check for changes to MXCSR for rounding modes, FTZ, or exception status/masks
        UI32 getInitialMXCSR() const { return m_initialMXCSR; }
        UI32 compareCurrentToInitialMXCSR() const { return m_initialMXCSR ^ getcsr(); }

#ifdef XPUINFO_USE_RAPIDJSON
        template <typename rjDocOrVal>
        DeviceCPU(const rjDocOrVal& val);
#endif

    protected:
        static UI32 getcsr();
        const UI32 m_initialMXCSR;
        std::shared_ptr<HybridDetect::PROCESSOR_INFO> m_pProcInfo;
    };

    class Device;
    typedef std::shared_ptr<Device> DevicePtr;

    class Device : public DeviceBase
    {
    public:
        Device(UI32 inIndex, DXGI_ADAPTER_DESC1* pDesc, 
            DeviceType inType=DEVICE_TYPE_GPU, APIType inAPI=API_TYPE_DXGI,
            // If non-zero, use this version instead of looking up based on LUID.
            // Required for deserialization.
            XI::UI64 rawDriverVerion=0ULL 
        );
        virtual ~Device();
        virtual WString name() const override { return m_props.dxgiDesc.Description; }
        virtual UI64 getLUID() const override { return LuidToUI64(m_props.dxgiDesc.AdapterLuid); }
        const LUID& getLUIDAsStruct() const { return m_props.dxgiDesc.AdapterLuid; }
        const DeviceDriverVersion& driverVersion() const;
        friend class XPUInfo;
        const DeviceProperties& getProperties() const { return m_props; };
        APIType getCurrentAPIs() const { return validAPIs; }
        ctl_device_adapter_handle_t getHandle_IGCL() const { return m_hIGCLAdapter; }
        ze_device_handle_t getHandle_L0() const { return m_L0Device; }
#ifdef _WIN32
        nvmlDevice_t getHandle_NVML() const { return m_nvmlDevice; }
        IDXCoreAdapter* const getHandle_DXCore() const { return m_pDXCoreAdapter.get(); }
#endif

        bool IsVendor(const UINT inVendorId) const { return m_props.dxgiDesc.VendorId == inVendorId; }

        DXCoreAdapterMemoryBudget getMemUsage() const;

#ifdef XPUINFO_USE_RAPIDJSON
        template <typename Alloc>
        rapidjson::Value serialize(Alloc& a);
        static DevicePtr deserialize(const rapidjson::Value& val);
#endif
        bool operator==(const Device& dev) const;

    protected:
        APIType validAPIs = API_TYPE_UNKNOWN;
        DeviceProperties m_props;

        // unique_ptr helps avoid unintentional copying, but not valid as member of vector
        SharedPtr<DeviceDriverVersion> m_pDriverVersion;
#ifdef _WIN32
        void initDXIntelPerfCounter(IDXGIAdapter1*);
#endif
        
        // Level Zero
        void initL0Device(ze_device_handle_t inL0Device, const ze_device_properties_t& device_properties, const L0_Extensions& exts);
        ze_device_handle_t m_L0Device = nullptr;

        // IGCL
        void initIGCLDevice(ctl_device_adapter_handle_t inHandle, IGCLAdapterPropertiesPtr& inPropsPtr);
        ctl_device_adapter_handle_t m_hIGCLAdapter = nullptr;
        String m_IGCLAdapterName;

        // OpenCL
        void initOpenCLDevice(cl_platform_id inPlatform, cl_device_id inDevice, const std::string& inExtensions);
        cl_device_id m_CLDevice = nullptr;
        cl_platform_id m_CLPlatform = nullptr;
        String m_OpenCLAdapterName;

        // DXCore
#ifdef _WIN32
        void initDXCoreDevice(winrt::com_ptr<IDXCoreAdapter>& pDXCoreAdapter, bool bHighPerf, bool bMinPower);
        winrt::com_ptr<IDXCoreAdapter> m_pDXCoreAdapter;
#endif
        DXCoreAdapterMemoryBudget getMemUsage_DXCORE() const;
        DXCoreAdapterMemoryBudget getMemUsage_Metal() const;

        // NVML
#ifdef _WIN32
        void initNVMLDevice(nvmlDevice_t device);
        nvmlDevice_t m_nvmlDevice = nullptr;
#endif
    };
    std::ostream& operator<<(std::ostream& ostr, const Device& xi);
    std::ostream& operator<<(std::ostream& ostr, const DevicePtr& xi);

    class ConstDevicePtrVec : public std::vector<DevicePtr>
    {
    public:
        ConstDevicePtrVec(const std::string& label = "Preferred Devices") : m_label(label) {}
        const std::string& getLabel() const { return m_label; }
        void setLabel(const std::string& label) { m_label = label; }
        friend std::ostream& operator<<(std::ostream& ostr, const ConstDevicePtrVec& devPtrs);

    protected:
        std::string m_label;
    };

#ifdef XPUINFO_USE_TELEMETRYTRACKER
    class TelemetryTracker : public NoCopyAssign
    {
    public:
        friend class Device;
        TelemetryTracker(const DevicePtr& deviceToTrack, UI32 msPeriod, std::ostream* pRealTimeOutputStream = nullptr);
        virtual ~TelemetryTracker() noexcept(false);

        void start();
        void stop();
        String getLog() const;

        UI64 getMaxMemUsage() const;
        UI64 getInitialMemUsage() const;

        const DevicePtr& getDevice() const;

        enum TelemetryItem : UI32
        {
            TELEMETRYITEM_UNKNOWN = 0,
            TELEMETRYITEM_FREQUENCY =   1 << 0,
            TELEMETRYITEM_READ_BW =     1 << 1,
            TELEMETRYITEM_WRITE_BW =    1 << 2,
            TELEMETRYITEM_GLOBAL_ACTIVITY =         1 << 3,
            TELEMETRYITEM_RENDER_COMPUTE_ACTIVITY = 1 << 4,
            TELEMETRYITEM_MEDIA_ACTIVITY =          1 << 5,

            TELEMETRYITEM_MEMORY_USAGE =            1 << 6,
            TELEMETRYITEM_TIMESTAMP_DOUBLE =        1 << 7, // Use double (i.e. from IGCL), else use UI64 from CPU

            TELEMETRYITEM_FREQUENCY_MEDIA =         1 << 8,
            TELEMETRYITEM_FREQUENCY_MEMORY =        1 << 9,

            // TODO: PCI bandwidth?  Neither IGCL nor L0 working now.  Can derive from micro+mem_bw, though.
        };

        struct TimedRecord
        {
            union
            {
                double timeStamp;
                UI64 timeStampUI64;
            };
            double freq;
            double freq_media;
            double freq_memory;

            // Device memory bandwidth
            UI64 bw_read;
            UI64 bw_write;

            // Resource usage
            UI64 deviceMemoryUsedBytes;
            UI64 deviceMemoryBudgetBytes;

            // Engine activity
            double activity_global;
            double activity_compute;
            double activity_media;

            double pctCPU;
            double cpu_freq;
        };
        typedef std::vector<TimedRecord> TimedRecords;

#ifdef _WIN32
        static VOID CALLBACK
            MyTimerCallback(
                PTP_CALLBACK_INSTANCE Instance,
                PVOID                 Parameter,
                PTP_TIMER             Timer
            )
        {
            UNREFERENCED_PARAMETER(Instance);
            UNREFERENCED_PARAMETER(Timer);
            TelemetryTracker* pClass = (TelemetryTracker*)Parameter;

            pClass->RecordNow();
        }
#endif

    protected:
        std::mutex m_RecordMutex;
        const DevicePtr m_Device; // Tracker will keep device "alive" if needed
        const DWORD m_msPeriod;
        TelemetryItem m_ResultMask;
        std::ostream* m_pRealtime_ostr;

#ifdef _WIN32
        PDH_HQUERY m_pdhQuery = nullptr;
        PDH_HCOUNTER m_pdhCtrCPU = nullptr;
        PDH_HCOUNTER m_pdhCtrCPUFreq = nullptr;
        PDH_HCOUNTER m_pdhCtrCPUPctPerf = nullptr;
        bool RecordCPU_PDH(TimedRecord& rec);
        void InitPDH();
#endif

        void InitL0();
        void InitIGCL();

        void RecordNow();
        bool RecordMemoryUsage(TimedRecord& rec);
        bool RecordIGCL(TimedRecord& rec);
        bool RecordNVML(TimedRecord& rec);
        bool RecordL0(TimedRecord& rec);
        bool RecordCPUTimestamp(TimedRecord& rec);
        void printRecord(TimedRecords::const_iterator it, std::ostream& ostr) const;
        void printRecordHeader(std::ostream& ostr) const;
#ifdef _WIN32
        PTP_TIMER m_timer = nullptr;
        TP_CALLBACK_ENVIRON m_CallBackEnviron;
        PTP_CLEANUP_GROUP m_cleanupgroup = nullptr;
#endif
        
        double m_startTime = 0.;
        UI64 m_startTimeUI64 = 0;
        UI64 m_timestamp_freq = 0;

        double m_freqMax = 0.;
        double m_freqMin = std::numeric_limits<double>::max();
        double m_freqMaxHW = 0.;
        double m_freqMinHW = 0.;
        std::vector<TimedRecord> m_records;
        std::vector<zes_freq_handle_t> m_freqHandlesL0;

        // IGCL, ctlFrequencyGetState
        ctl_freq_handle_t m_IGCL_MemFreqHandle = nullptr;
    };

    class TelemetryTrackerWithScopedLog : public TelemetryTracker
    {
    public:
        TelemetryTrackerWithScopedLog(const DevicePtr& deviceToTrack, UI32 msPeriod, 
            std::ostream& logStream, std::ostream* pRealTimeOutputStream = nullptr):
            TelemetryTracker(deviceToTrack, msPeriod, pRealTimeOutputStream),
            m_ostr(logStream) {}
        virtual ~TelemetryTrackerWithScopedLog() noexcept(false)
        {
            stop();
            m_ostr << getLog();
        }

    protected:
        std::ostream& m_ostr;
    };
#endif // XPUINFO_USE_TELEMETRYTRACKER

    typedef struct _InvalidAPIType InvalidAPIType;
    template <APIType APITYPE>
    class API_Traits
    {
    public:
        const APIType kAPIType = API_TYPE_UNKNOWN;
        typedef InvalidAPIType API_handle_type;
    };

    template <>
    class API_Traits<API_TYPE_LEVELZERO>
    {
    public:
        const APIType kAPIType = API_TYPE_LEVELZERO;
        typedef ze_device_handle_t API_handle_type;
    };

    template <>
    class API_Traits<API_TYPE_IGCL>
    {
    public:
        const APIType kAPIType = API_TYPE_IGCL;
        typedef ctl_device_adapter_handle_t API_handle_type;
    };
    
    class SetupDeviceInfo
    {
    public:
        SetupDeviceInfo();
        ~SetupDeviceInfo();

        const DriverInfoPtr getByLUID(const UI64 inLUID) const;
        const DriverInfoPtr getAtAddress(const PCIAddressType& inAddress) const;
        const DriverInfoPtr getByName(const WString& inName) const;

    protected:
        std::vector<DriverInfoPtr> m_DevInfoPtrs;
    };

    class SystemInfo
    {
    public:
        SystemInfo();
#ifdef XPUINFO_USE_RAPIDJSON
        template <typename rjDocOrVal>
        SystemInfo(const rjDocOrVal& val);
        template <typename Alloc>
        rapidjson::Value serialize(Alloc& a) const;
#endif

        // Win32_ComputerSystem
        WString Manufacturer;
        WString Model;
        I32 NumberOfLogicalProcessors;
        I32 NumberOfProcessors;
        WString SystemFamily;
        WString SystemSKUNumber;
        WString SystemType;
        I64 TotalPhysicalMemory = -1;

        // Win32_OperatingSystem
        struct OSInfo
        {
            WString BuildNumber;
            WString Caption; // Descriptive name
            WString CodeSet;
            WString CountryCode;
            WString Name;
            UI64 FreePhysicalMemoryKB = 0ULL;
            UI64 FreeSpaceInPagingFilesKB = 0ULL;
            UI64 FreeVirtualMemoryKB = 0ULL;
            UI64 TotalVirtualMemorySizeKB = 0ULL;
            UI64 TotalVisibleMemorySizeKB = 0ULL;
            WString LastBootUpDate;
            WString LocalDate;
            WString Locale;
            WString OSArchitecture;
            UI32 OSLanguage = 0;
            UI32 getUptimeDays() const;
        };
        OSInfo OS;

        // Win32_BIOS
        struct BIOSInfo
        {
            WString Name;
            WString Manufacturer;
            WString SerialNumber;
            WString Version;
            WString ReleaseDate;
        };
        BIOSInfo BIOS;

        struct Processor
        {
            I32 ClockSpeedMHz = -1;  // Nominal
            I32 NumberOfCores = -1;
            I32 NumberOfEnabledCores = -1;
            I32 NumberOfLogicalProcessors = -1;
            bool operator!=(const Processor& p) const
            {
                return (ClockSpeedMHz != p.ClockSpeedMHz) ||
                    (NumberOfCores != p.NumberOfCores) ||
                    (NumberOfEnabledCores != p.NumberOfEnabledCores) ||
                    (NumberOfLogicalProcessors != p.NumberOfLogicalProcessors);
            }
        };
        std::vector<Processor> Processors;

        struct MemoryDeviceInfo
        {
            UI32 SpeedMHz;
            UI64 Capacity;
            bool operator<(const MemoryDeviceInfo& rhs) const
            {
                return (SpeedMHz < rhs.SpeedMHz) || (Capacity < rhs.Capacity);
            }
        };

        WString getMemoryDescription() const;
        // Only returns speed of first type - consider invalid if >1 type
        UI32 getMemorySpeed() const;
        // Usually systems have just 1 uniform capacity and speed of memory
        UI32 getMemoryTypeCount() const { return (UI32)m_mapMemSize.size(); }
        // Total number of memory devices across all types
        UI32 getMemoryDeviceCount() const;

        struct VideoControllerInfo
        {
            WString Name;
            WString VideoMode;
            UI32 RefreshRate;
            WString InfSection; // Needed?
            WString PNPDeviceID; // To match with SetupAPI
        };
        std::vector<VideoControllerInfo> VideoControllers;

        friend std::ostream& operator<<(std::ostream& os, const SystemInfo& si);

    protected:
        std::map<MemoryDeviceInfo, int> m_mapMemSize; // {info, device_count}
    };

    struct RuntimeVersion
    {
        UI32 major, minor, build;
        String productVersion;
        String getAsString() const;
        bool operator!=(const RuntimeVersion& l) const;
        bool operator==(const RuntimeVersion& l) const;
        RuntimeVersion() : major(0), minor(0), build(0) {}
#ifdef XPUINFO_USE_RAPIDJSON
        RuntimeVersion(const rapidjson::Value& val);
#endif
    };

#ifdef XPUINFO_USE_SYSTEMEMORYINFO
    // Basic OS-provided memory information
    class SystemMemoryInfo
    {
    public:
        SystemMemoryInfo(const std::shared_ptr<SystemInfo> pSysInfo = nullptr);
#ifdef XPUINFO_USE_RAPIDJSON
        template <typename rjDocOrVal>
        SystemMemoryInfo(const rjDocOrVal& val);
#endif

        static size_t getCurrentAvailablePhysicalMemory();
        static size_t getCurrentTotalPhysicalMemory(); // Should not change over time - just static impl
        static size_t getCurrentInstalledPhysicalMemory(); // Should not change over time - just static impl
        size_t getInstalledPhysicalMemory() const { return m_installedPhysicalMemory; }
        size_t getTotalPhysicalMemory() const { return m_totalPhysicalMemory; }
        size_t getAvailablePhysicalMemoryAtInit() const { return m_availablePhysicalMemoryAtInit; }
        size_t getPageSize() const { return m_pageSize; }

    protected:
        size_t m_installedPhysicalMemory = 0;
        size_t m_totalPhysicalMemory=0;
        size_t m_availablePhysicalMemoryAtInit=0;
        size_t m_pageSize = 0;
        // For later use since some overlap on some OSs
        //const std::shared_ptr<SystemInfo> m_pSysInfo;
    };
#endif // XPUINFO_USE_SYSTEMEMORYINFO

    class XPUInfo;
    typedef std::shared_ptr<XPUInfo> XPUInfoPtr;

    class XPUInfo
    {
    public:
        // Constructor compares class size of client to that of lib to help verify that 
        // preprocessor arguments are in agreement between different projects.
        XPUInfo(APIType initMask, size_t classSize = sizeof(XPUInfo));
        ~XPUInfo();
        size_t deviceCount() const { return m_Devices.size(); }
        template <APIType APITYPE>
        bool getDevice(UI64 inLUID, typename API_Traits<APITYPE>::API_handle_type* outTypePtr);
        typedef std::map<UI64, DevicePtr> DeviceMap;
        const DevicePtr getDevice(UI64 inLUID) const;
        const DevicePtr getDevice(const char* inNameSubString) const;
        const DevicePtr getDeviceByIndex(UI32 inIndex) const;
        const DeviceMap& getDeviceMap() const { return m_Devices; }
        const DeviceCPU& getCPUDevice() const;

        void printInfo(std::ostream& ostr) const;
        void printCPUInfo(std::ostream& ostr) const;
        void printSystemInfo(std::ostream& ostr) const;
        void printSystemMemoryInfo(std::ostream& ostr) const;
        const SystemInfo* getSystemInfo() const { return m_pSystemInfo.get(); }

        static bool hasDXCore();

#ifdef _WIN32
        const winrt::com_ptr<IDXCoreAdapterFactory>& getDXCoreFactory() const { return m_spFactoryDXCore; }
        // TODO: Change to also handle NPUs only in m_spAdapterList2
        const winrt::com_ptr<IDXCoreAdapterList>& getDXCoreAdapterList() const { return m_spAdapterList; }
#endif

        typedef std::unordered_map<String, RuntimeVersion> RuntimeVersionInfoMap;
#ifdef XPUINFO_USE_RUNTIMEVERSIONINFO
        const RuntimeVersionInfoMap& getRuntimeVersionInfo() const { return m_RuntimeVersions; }
#endif
#ifdef XPUINFO_USE_SYSTEMEMORYINFO
        const std::shared_ptr<SystemMemoryInfo>& getSystemMemoryInfoPtr() const { return m_pMemoryInfo; }
#endif

#ifdef XPUINFO_USE_RAPIDJSON
        // Serialize to top-level rapidjson::Document or rapidjson::Value
        // Not all fields serialized
        template <typename rjDocOrVal, typename Alloc>
        bool serialize(rjDocOrVal& val, Alloc& a);
        // Full deserialize not implemented - see Device::deserialize
        template <typename rjDocOrVal>
        static XPUInfoPtr deserialize(const rjDocOrVal& val);
#endif
        APIType getUsedAPIs() const { return m_UsedAPIs; }

    private:
        DevicePtr getDeviceInternal(UI64 inLUID);
        DevicePtr getDeviceInternal(const char* inNameSubString);
        void initCPU();
#ifdef _WIN32
        void initDXGI(APIType initMask);
        void initDXCore();
        void initIGCL(bool useL0);
        void initL0();
        void initOpenCL();
        void initNVML();
        void initWMI();
        void shutdownNVML();
#elif __APPLE__
        void initMetal();
#endif
        void finalInitDXGI();
        DeviceMap m_Devices;
        APIType m_UsedAPIs;
        std::shared_ptr<SystemInfo> m_pSystemInfo;
#ifdef XPUINFO_USE_SYSTEMEMORYINFO
        std::shared_ptr<SystemMemoryInfo> m_pMemoryInfo;
#endif // XPUINFO_USE_SYSTEMEMORYINFO
#ifdef _WIN32
        std::shared_ptr<SetupDeviceInfo> m_pSetupInfo;
        winrt::com_ptr<IDXCoreAdapterFactory> m_spFactoryDXCore;
        winrt::com_ptr<IDXCoreAdapterList> m_spAdapterList;
        winrt::com_ptr<IDXCoreAdapterList> m_spAdapterList2;
#endif
        std::shared_ptr<DeviceCPU> m_pCPU;

#ifdef XPUINFO_USE_RUNTIMEVERSIONINFO
        void getRuntimeVersions();
        RuntimeVersionInfoMap m_RuntimeVersions;
#endif
    };
    std::ostream& operator<<(std::ostream& ostr, const XPUInfo& xi);

    // ScopedRegisterNotification inteded to work as no-op when DXCORE is not available
    class ScopedRegisterNotification : public NoCopyAssign
    {
    public:
        enum TypeFlags
        {
            NONE = 0,
            DXCORE_ADAPTER_STATE = 1,
            DXCORE_MEM_BUDGET = 1 << 1,
        };

#ifdef _WIN32
        typedef std::function<void(DXCoreNotificationType notificationType, IUnknown* object, const XI::XPUInfo* pXI)> DXCoreNotificationFunc;
        // Provide your own callback, or use ExampleNotificationFunc for testing.
        static void ExampleNotificationFunc(DXCoreNotificationType notificationType, IUnknown* object, const XI::XPUInfo* pXI); // Always exists
        static void ExampleNotificationFunc_DXCORE(DXCoreNotificationType notificationType, IUnknown* object, const XI::XPUInfo* pXI); // Only exists if XPUINFO_USE_DXCORE defined
        static inline const TypeFlags defaultFlags = TypeFlags(TypeFlags::DXCORE_ADAPTER_STATE | TypeFlags::DXCORE_MEM_BUDGET);
#else
        typedef std::function<void()> DXCoreNotificationFunc;
        static void ExampleNotificationFunc();
        static inline const TypeFlags defaultFlags = TypeFlags::NONE;
#endif

        ScopedRegisterNotification(UI64 deviceLUID, const XPUInfo* pXI, TypeFlags flags, const DXCoreNotificationFunc& callbackFunc = ExampleNotificationFunc);
        ~ScopedRegisterNotification() noexcept(false);

        static std::mutex& getMutex() { return m_NotificationMutex; }


    protected:
        static std::mutex m_NotificationMutex;
        const XPUInfo* m_pXI;
        const DXCoreNotificationFunc m_NotificationFunc;
        const TypeFlags m_flags;

#ifdef _WIN32
        void register_DXCORE(UI64 deviceLUID);
        void unregister_DXCORE();
        static void DXCoreNotificationCallback(DXCoreNotificationType notificationType,
            IUnknown* object,
            void* context);
#endif
        std::array<uint32_t, 4> m_DXCoreEventCookies;
        bool m_bRegisteredEvents = false;
        bool m_bRegisteredAdapterBudgetChange = false;
    };


}; // XI