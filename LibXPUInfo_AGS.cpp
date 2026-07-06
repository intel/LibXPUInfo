// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_AGS
#define BUILD_AGS_SAMPLE 0
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#pragma warning(push)
#pragma warning(disable : 4471)
#include "external\AGS_SDK\ags_lib\inc\amd_ags.h"
#pragma warning(pop)
#ifdef NDEBUG
#ifdef _DLL // Assumes MSVC compiler
#pragma comment(lib, "amd_ags_x64_2022_MD.lib")
#else
#pragma comment(lib, "amd_ags_x64_2022_MT.lib")
#endif
#else
#ifdef _DLL // Assumes MSVC compiler
#pragma comment(lib, "amd_ags_x64_2022_MDd.lib")
#else
#pragma comment(lib, "amd_ags_x64_2022_MTd.lib")
#endif
#endif

namespace
{
    const char* AGS_asicFamily[] =
    {
        "unknown",
        "Pre GCN",
        "GCN Gen1",
        "GCN Gen2",
        "GCN Gen3",
        "GCN Gen4",
        "Vega",
        "RDNA",
        "RDNA2",
        "RDNA3",
        "RDNA4"
    };

    static_assert(_countof(AGS_asicFamily) == AGSAsicFamily_Count, "asic family table out of date");
#if BUILD_AGS_SAMPLE
    const char* getVendorName(int vendorId)
    {
        switch (vendorId)
        {
        case 0x1002: return "AMD";
        case 0x8086: return "INTEL";
        case 0x10DE: return "NVIDIA";
        default: return "unknown";
        }
    }


    void PrintDisplayInfo(const AGSGPUInfo& gpuInfo)
    {
        for (int gpuIndex = 0; gpuIndex < gpuInfo.numDevices; gpuIndex++)
        {
            const AGSDeviceInfo& device = gpuInfo.devices[gpuIndex];

            printf("\n---------- Device %d%s, %s\n", gpuIndex, device.isPrimaryDevice ? " [primary]" : "", device.adapterString);

            printf("Vendor id:   0x%04X (%s)\n", device.vendorId, getVendorName(device.vendorId));
            printf("Device id:   0x%04X\n", device.deviceId);
            printf("Revision id: 0x%04X\n\n", device.revisionId);



            if (device.vendorId == 0x1002)
            {
                char wgpInfo[256] = {};
                if (device.asicFamily >= AGSAsicFamily_RDNA)
                {
                    sprintf_s(wgpInfo, ", %d WGPs", device.numWGPs);
                }

                printf("Architecture: %s, %s%s%d CUs%s, %d ROPs\n", AGS_asicFamily[device.asicFamily], device.isAPU ? "(APU), " : "", device.isExternal ? "(External), " : "", device.numCUs, wgpInfo, device.numROPs);
                printf("    core clock %d MHz, memory clock %d MHz\n", device.coreClock, device.memoryClock);
                printf("    %.1f Tflops\n", device.teraFlops);
                printf("local memory: %d MBs (%.1f GB/s), shared memory: %d MBs\n\n", (int)(device.localMemoryInBytes / (1024 * 1024)), (float)device.memoryBandwidth / 1024.0f, (int)(device.sharedMemoryInBytes / (1024 * 1024)));
            }

            printf("\n");

            if (device.eyefinityEnabled)
            {
                printf("SLS grid is %d displays wide by %d displays tall\n", device.eyefinityGridWidth, device.eyefinityGridHeight);
                printf("SLS resolution is %d x %d pixels%s\n", device.eyefinityResolutionX, device.eyefinityResolutionY, device.eyefinityBezelCompensated ? ", bezel-compensated" : "");
            }
            else
            {
                printf("Eyefinity not enabled on this device\n");
            }

            printf("\n");

            for (int i = 0; i < device.numDisplays; i++)
            {
                const AGSDisplayInfo& display = device.displays[i];

                printf("\t---------- Display %d %s----------------------------------------\n", i, display.isPrimaryDisplay ? "[primary]" : "---------");

                printf("\tdevice name: %s\n", display.displayDeviceName);
                printf("\tmonitor name: %s\n\n", display.name);

                printf("\tMax resolution:             %d x %d, %.1f Hz\n", display.maxResolutionX, display.maxResolutionY, display.maxRefreshRate);
                printf("\tCurrent resolution:         %d x %d, Offset (%d, %d), %.1f Hz\n", display.currentResolution.width, display.currentResolution.height, display.currentResolution.offsetX, display.currentResolution.offsetY, display.currentRefreshRate);
                printf("\tVisible resolution:         %d x %d, Offset (%d, %d)\n\n", display.visibleResolution.width, display.visibleResolution.height, display.visibleResolution.offsetX, display.visibleResolution.offsetY);

                printf("\tchromaticity red:           %f, %f\n", display.chromaticityRedX, display.chromaticityRedY);
                printf("\tchromaticity green:         %f, %f\n", display.chromaticityGreenX, display.chromaticityGreenY);
                printf("\tchromaticity blue:          %f, %f\n", display.chromaticityBlueX, display.chromaticityBlueY);
                printf("\tchromaticity white point:   %f, %f\n\n", display.chromaticityWhitePointX, display.chromaticityWhitePointY);

                printf("\tluminance: [min, max, avg]  %f, %f, %f\n", display.minLuminance, display.maxLuminance, display.avgLuminance);

                printf("\tscreen reflectance diffuse  %f\n", display.screenDiffuseReflectance);
                printf("\tscreen reflectance specular %f\n\n", display.screenSpecularReflectance);

                if (display.HDR10)
                    printf("\tHDR10 supported\n");

                if (display.dolbyVision)
                    printf("\tDolby Vision supported\n");

                if (display.freesync)
                    printf("\tFreesync supported\n");

                if (display.freesyncHDR)
                    printf("\tFreesync HDR supported\n");

                printf("\n");

                if (display.eyefinityInGroup)
                {
                    printf("\tEyefinity Display [%s mode] %s\n", display.eyefinityInPortraitMode ? "portrait" : "landscape", display.eyefinityPreferredDisplay ? " (preferred display)" : "");

                    printf("\tGrid coord [%d, %d]\n", display.eyefinityGridCoordX, display.eyefinityGridCoordY);
                }

                printf("\tlogical display index: %d\n", display.logicalDisplayIndex);
                printf("\tADL adapter index: %d\n\n", display.adlAdapterIndex);

                printf("\n");
            }
        }
    }


    void testRadeonSoftwareVersion(const char* driver, unsigned int driverToCompareAgainst)
    {
        AGSDriverVersionResult result = agsCheckDriverVersion(driver, driverToCompareAgainst);

        int major = (driverToCompareAgainst & 0xFFC00000) >> 22;
        int minor = (driverToCompareAgainst & 0x003FF000) >> 12;
        int patch = (driverToCompareAgainst & 0x00000FFF);

        if (result == AGS_SOFTWAREVERSIONCHECK_UNDEFINED)
        {
            printf("Driver check could not determine the driver version for %s\n", driver);
        }
        else
        {
            printf("Driver check shows the installed %s driver is %s the %d.%d.%d required version\n", driver, result == AGS_SOFTWAREVERSIONCHECK_OK ? "newer or the same as" : "older than", major, minor, patch);
        }
    }


    int ags_sample_main(int, char**)
    {
        // Enable run-time memory check for debug builds.
        // (When _DEBUG is not defined, calls to _CrtSetDbgFlag are removed during preprocessing.)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

        //int displayIndex = 0;
        //DISPLAY_DEVICEA displayDevice = {};
        //displayDevice.cb = sizeof(displayDevice);
        //while (EnumDisplayDevicesA(0, displayIndex, &displayDevice, 0))
        //{
        //    printf("Display Device: %d: %s, %s %s%s\n", displayIndex, displayDevice.DeviceString, displayDevice.DeviceName, displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE ? "(primary)" : "", displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE ? "" : " [disabled]");
        //    displayIndex++;
        //}

        AGSContext* agsContext = nullptr;
        AGSGPUInfo gpuInfo = {};
        AGSConfiguration config = {};
        if (agsInitialize(AGS_CURRENT_VERSION, &config, &agsContext, &gpuInfo) == AGS_SUCCESS)
        {
            printf("\nAGS Library initialized: v%d.%d.%d\n", AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH);
            printf("-----------------------------------------------------------------\n");

            printf("Radeon Software Version:   %s\n", gpuInfo.radeonSoftwareVersion);
            printf("Driver Version:            %s\n", gpuInfo.driverVersion);
            printf("-----------------------------------------------------------------\n");
            PrintDisplayInfo(gpuInfo);
            printf("-----------------------------------------------------------------\n");

            if (0)
            {
                // It should be noted that the Radeon Software Version can be empty, or just have the internal driver string in this field.
                // Therefore, it is recommended to use the internal driver version as a minimum driver version check.
                printf("\n");
                testRadeonSoftwareVersion(gpuInfo.radeonSoftwareVersion, AGS_MAKE_VERSION(24, 1, 1));
                testRadeonSoftwareVersion("24.1.randombetadriver", AGS_MAKE_VERSION(24, 1, 1));
                testRadeonSoftwareVersion("24.1.123randomdriver", AGS_MAKE_VERSION(24, 1, 1));
                testRadeonSoftwareVersion("24.2.randomdriver", AGS_MAKE_VERSION(24, 1, 1));
                testRadeonSoftwareVersion("24.Q1", AGS_MAKE_VERSION(24, 1, 1));
                testRadeonSoftwareVersion("24.1.1", AGS_MAKE_VERSION(24, 1, 1));
                testRadeonSoftwareVersion("24.1.1", AGS_MAKE_VERSION(23, 12, 1));
                testRadeonSoftwareVersion("24.1.1", AGS_MAKE_VERSION(25, 2, 4));
                printf("\n");
            }

            if (agsDeInitialize(agsContext) != AGS_SUCCESS)
            {
                printf("Failed to cleanup AGS Library\n");
            }
        }
        else
        {
            printf("Failed to initialize AGS Library\n");
        }

        printf("\ndone\n");

        return 0;
    }
#endif // BUILD_AGS_SAMPLE
} // private

namespace XI
{
    int ags_sample()
    {
#if BUILD_AGS_SAMPLE
        return ags_sample_main(0, nullptr);
#else
        return 0;
#endif
    }

    class AGSContainer {
    public:
        AGSContainer();
        ~AGSContainer();
        bool isInitialized() const { return m_initialized; }
        const AGSGPUInfoPtr& getGPUInfo() const { return m_gpuInfoPtr; }

    private:
        AGSContext* m_agsContext;
        AGSGPUInfoPtr m_gpuInfoPtr;
        bool m_initialized;
    };

    AGSContainer::AGSContainer()
        : m_agsContext(nullptr)
        , m_gpuInfoPtr(std::make_unique<AGSGPUInfo>())
        , m_initialized(false)
    {
        AGSConfiguration config = {};
        if (agsInitialize(AGS_CURRENT_VERSION, &config, &m_agsContext, m_gpuInfoPtr.get()) == AGS_SUCCESS)
        {
            m_initialized = true;
        }
    }

    AGSContainer::~AGSContainer()
    {
        if (m_initialized && m_agsContext)
        {
            agsDeInitialize(m_agsContext);
            m_agsContext = nullptr;
        }
    }

    void Device::initAGSDevice(int gpuIndex, const AGSGPUInfoPtr& pAGSAdapterInfo)
    {
        if (pAGSAdapterInfo && (pAGSAdapterInfo->numDevices > gpuIndex))
        {
            auto& agsDevice = pAGSAdapterInfo->devices[gpuIndex];

            m_props.NumComputeUnits = agsDevice.numCUs;
            m_props.ComputeUnitSIMDWidth = (agsDevice.asicFamily >= AGSAsicFamily_RDNA) ? 32 : 16; // TODO: confirm
            updateIfDstNotSet(m_props.FreqMaxMHz, (I32)agsDevice.coreClock);

            if (m_props.DeviceGenerationAPI == API_TYPE_UNKNOWN)
            {
                m_props.DeviceGenerationAPI = API_TYPE_AGS;
                m_props.DeviceGenerationID = agsDevice.asicFamily;
            }
            validAPIs = validAPIs | API_TYPE_AGS;
        }
    }
    void XPUInfo::initAGS()
    {
        AGSContainer agsContainer;
        const auto& gpuInfoPtr = agsContainer.getGPUInfo();
        if (agsContainer.isInitialized() && gpuInfoPtr)
        {
            for (int gpuIndex = 0; gpuIndex < gpuInfoPtr->numDevices; gpuIndex++)
            {
                const AGSDeviceInfo& device = gpuInfoPtr->devices[gpuIndex];
                // Find matching XI::Device by name
                for (auto& [luid, dev] : m_Devices)
                {
                    if (strcmp(device.adapterString, convert(dev->name()).c_str()) == 0)
                    {
                        dev->initAGSDevice(gpuIndex, gpuInfoPtr);
                        m_UsedAPIs = m_UsedAPIs | API_TYPE_AGS;
                        break;
                    }
                }
            }
        }
    }

    const char* getGenerationName_AGS(I32 agsAsicFamily)
    {
        if ((agsAsicFamily>=0) && (agsAsicFamily < AGSAsicFamily_Count))
        {
            return AGS_asicFamily[agsAsicFamily];
        }
        return AGS_asicFamily[0];
    }
} // XI
#endif // XPUINFO_USE_AGS
