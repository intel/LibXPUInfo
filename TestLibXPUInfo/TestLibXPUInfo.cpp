// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef TESTLIBXPUINFO_STANDALONE
#define TESTLIBXPUINFO_STANDALONE 1
#endif
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#include "LibXPUInfo_JSON.h"
#include "utility/LibXPUInfo_D3D12Utility.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace XI;

#ifdef XPUINFO_USE_RAPIDJSON
bool getXPUInfoJSON(std::ostream& ostr, const XI::XPUInfoPtr& pXI)
{
    if (!ostr.fail())
    {
        rapidjson::Document doc;
        doc.SetObject();

        if (pXI->serialize(doc))
        {
            rapidjson::OStreamWrapper out(ostr);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(out);
            doc.Accept(writer);    // Accept() traverses the DOM and generates Handler events.
            ostr << std::endl;
            return true;
        }
    }
    return false;
}

bool testWriteJSON(const std::filesystem::path jsonPath)
{
    std::filesystem::create_directories(std::filesystem::absolute(jsonPath).parent_path()); // Create if needed
    std::ofstream jf(jsonPath);
    APIType apis = APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI);
    std::cout << "Initializing XPUInfo with APIType = " << apis << "...\n";
    XI::XPUInfoPtr pXI(new XPUInfo(apis));

    bool result = getXPUInfoJSON(jf, pXI);
    if (!result)
    {
        std::cout << "Error writing " << std::filesystem::absolute(jsonPath) << std::endl;
    }
    else
    {
        std::cout << "Wrote " << std::filesystem::absolute(jsonPath) << std::endl;
    }
    return result;
}

bool testVerifyJSON()
{
    std::ostringstream ostr;
    APIType apis = APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI);
    std::cout << "Initializing XPUInfo with APIType = " << apis << "...\n";
    XI::XPUInfoPtr pXI(new XPUInfo(apis));
    if (getXPUInfoJSON(ostr, pXI))
    {
        std::istringstream istr(ostr.str());
        if (!istr.bad())
        {
            rapidjson::IStreamWrapper isw(istr);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            if (doc.HasParseError())
            {
                std::cout << "Error  : " << doc.GetParseError() << ": "
                    << "Offset : " << doc.GetErrorOffset() << '\n';
                return false;
            }
            XI::XPUInfoPtr pXID(XI::XPUInfo::deserialize(doc));
#ifdef _WIN32
            pXID->initDXCore(true);
            for (const auto& [luid, pDev] :pXID->getDeviceMap())
            {
                if (pDev->getCurrentAPIs() & API_TYPE_DXCORE)
                {
                    XPUINFO_REQUIRE(pDev->getHandle_DXCore());
                }
            }
#endif
            bool xiEqual = JSON::compareXI(pXI, pXID);
            if (!xiEqual)
            {
                std::cout << "XPUInfo comparison failed!\n";
            }
            else
            {
                std::cout << "XPUInfo comparison matched!\n";
            }
            return xiEqual;
        }
    }
    else
    {
        std::cout << "Failed to get XPUInfo JSON!\n";
    }
    return false;
}

bool testReadJSON(const std::filesystem::path jsonPath)
{
    if (std::filesystem::directory_entry(jsonPath).is_regular_file())
    {
        std::ifstream istr(jsonPath);
        if (!istr.bad())
        {
            rapidjson::IStreamWrapper isw(istr);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            if (doc.HasParseError())
            {
                std::cout << "Error  : " << doc.GetParseError() << ": "
                    << "Offset : " << doc.GetErrorOffset() << '\n';
                return false;
            }
            XI::XPUInfoPtr pXID(XI::XPUInfo::deserialize(doc));
            std::cout << *pXID << std::endl;
            return true;
        }
    }
    else
    {
        std::cout << "File not found: " << std::filesystem::absolute(jsonPath).string() << std::endl;
    }
    return false;
}
#endif

#ifdef _WIN32
class ScopedD3D12MemoryAllocation
{
public:
    ScopedD3D12MemoryAllocation(IUnknown* adapter, double sizeInGB)
    {
        size_t sizeInBytes = static_cast<size_t>(sizeInGB * 1024 * 1024 * 1024);
        if (!CreateD3D12DeviceAndAllocateResource(adapter, sizeInBytes, m_gpuMem))
        {
            throw std::runtime_error("Failed to allocate D3D12 memory");
        }

    }
protected:
    std::list<Microsoft::WRL::ComPtr<ID3D12Resource>> m_gpuMem;
};

static const XI::RuntimeNames runtimes = {
    "Microsoft.AI.MachineLearning.dll", "DirectML.dll", "onnxruntime.dll", "OpenVino.dll",
    "onnxruntime_providers_shared.dll", "onnxruntime_providers_openvino.dll",
};

int testInflateGPUMem(double sizeInGB, const std::string& devName)
{
    try
    {
        XPUInfo xi(XPUINFO_INIT_ALL_APIS, runtimes);
        auto xiDev = xi.getDevice(devName.c_str());
        if (xiDev)
        {
            std::cout << xi << std::endl << std::endl;
            std::cout << "Allocating " << sizeInGB << " GB on " << XI::convert(xiDev->name()) << std::endl;
            auto devHandle = xiDev->getHandle_DXCore();
            XPUINFO_REQUIRE(devHandle);
            try
            {
                ScopedD3D12MemoryAllocation mem(devHandle, sizeInGB);
                std::cout << "Press any key to continue...\n";
                int c = getchar(); (void)c;
            }
            catch (const std::exception& e)
            {
                std::cout << "Exception near ScopedD3D12MemoryAllocation: " << e.what() << std::endl;
                return -1;
            }
        }
        else
        {
            std::cout << "Device not found: " << devName << std::endl;
            return -1;
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception attempting inflate_gpu_mem: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
#else
static const XI::RuntimeNames runtimes; // empty for now
#endif

#ifdef XPUINFO_USE_TELEMETRYTRACKER
int runTelemetry(XI::UI32 telemInterval_ms, XI::UI32 telem_gpu_idx, bool peakOnly)
{
    // Find desired GPU, then start running
    APIType apis = APIType(XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI \
        | XI::API_TYPE_DX11_INTEL_PERF_COUNTER \
        | XI::API_TYPE_LEVELZERO \
        | XI::API_TYPE_IGCL_L0 | XI::API_TYPE_IGCL \
        | XI::API_TYPE_DXCORE | XI::API_TYPE_NVML);

    XI::XPUInfo xi(apis, runtimes);

    auto telemDevice = xi.getDeviceByIndex(telem_gpu_idx);
    {
        std::unique_ptr<XI::TelemetryTracker> tt;
        
        if (peakOnly)
        {
            tt = std::make_unique<XI::TelemetryTracker>(telemDevice, telemInterval_ms, nullptr, XI::TelemetryTracker::TELEMETRYITEM_PEAKUSAGE_ONLY);
        }
        else
        {
            tt = std::make_unique<XI::TelemetryTrackerWithScopedLog>(telemDevice, telemInterval_ms, std::cout);
        }
        XPUINFO_REQUIRE(tt);
        std::cout << "Telemetry started on device " << XI::convert(telemDevice->name()) << " with " << telemInterval_ms << " ms interval.\n";
        std::cout << "Press any key to stop...\n";
        tt->start();
        auto c = getchar();
        (void)c;

        std::cout << std::right << std::setw(40) << "Memory usage summary for device: " << XI::convert(telemDevice->name()) << std::endl;
        auto peak = tt->getPeakUsage();
        auto initial = tt->getInitialUsage();
        XI::SaveRestoreIOSFlags sr(std::cout);
        auto printPeak = [](const std::string& label, const XI::TelemetryTracker::PeakUsage& peak) {
            std::cout.precision(2);
            std::cout << std::fixed;
            constexpr double BYTES_TO_GB = 1024.0 * 1024.0 * 1024.0;
            std::cout << std::right << std::setw(40) << (label + " Device Mem (GB): ") << peak.deviceMemoryUsedBytes / BYTES_TO_GB << std::endl;
            std::cout << std::right << std::setw(40) << (label + " Device Mem, All Processes (GB): ") << peak.gpu_mem_Adapter_Total / BYTES_TO_GB << std::endl;
            std::cout << std::right << std::setw(40) << (label + " Shared Device Mem (GB): ") << peak.gpu_mem_Adapter_Shared / BYTES_TO_GB << std::endl;
            std::cout << std::right << std::setw(40) << (label + " Dedicated Device Mem (GB): ") << peak.gpu_mem_Adapter_Dedicated / BYTES_TO_GB << std::endl;
            };
        printPeak("Peak", peak);
        printPeak("Initial", initial);
        tt.reset(); // Stop telemetry and print results
    }
    std::cout << std::endl << xi << std::endl;
    return 0;
}
#endif

#if TESTLIBXPUINFO_STANDALONE
int main(int argc, char* argv[])
#else
int printXPUInfo(int argc, char* argv[])
#endif
{
    bool testIndividual = false;
    bool bRunTelemetry = false;
    bool peakOnly = false;
    XI::UI32 telemInterval_ms = 0;
    XI::UI32 telem_gpu_idx = 0;
    APIType additionalAPIs = APIType(0);
    APIType apiMask = APIType(0);

    for (int a = 1; a < argc; ++a)
    {
        String arg(argv[a]);
#ifdef _WIN32
        if (arg == "-1")
        {
            testIndividual = true;
        }
#endif
        if (arg == "-telemetry") // -telemetry <ms> [<gpuIndex>]
        {
            if ((a + 1 < argc))
            {
                std::istringstream istr(argv[++a]);
                XI::UI32 msInterval = 0;
                istr >> msInterval;
                if (!istr.bad())
                {
                    telemInterval_ms = msInterval;
                    bRunTelemetry = true;
                }
            }
            if ((a + 1 < argc) && (strlen(argv[a+1]) > 0) && (argv[a+1][0] != '-'))
            {
                std::istringstream istr(argv[++a]);
                XI::UI32 gpuIdx;
                istr >> gpuIdx;
                if (!istr.bad())
                {
                    telem_gpu_idx = gpuIdx;
                }
            }
        }
        else if (arg == "-peak_only")
        {
            peakOnly = true;
        }
        else if (arg == "-igcl_l0_enable")
        {
            additionalAPIs |= XI::API_TYPE_IGCL_L0;
        }
        // If specified, this takes precedence over additionalAPIs
        else if ((arg == "-apis") && (a+1 < argc))
        {
            std::underlying_type_t<APIType> inMask;
            std::istringstream istr(argv[++a]);
            istr >> std::hex >> inMask;
            if (!istr.bad())
            {
                apiMask = static_cast<APIType>(inMask);
            }
        }
#ifdef _WIN32
        // -inflate_gpu_mem 10.5 Arc
        else if ((arg == "-inflate_gpu_mem") && (a + 2 < argc))
        {
            double sizeInGB;
            std::istringstream istr(argv[++a]);
            std::string devName(argv[++a]);
            istr >> sizeInGB;
            if (!istr.fail())
            {
                return testInflateGPUMem(sizeInGB, devName);
            }
            else
            {
                std::cout << "Argument error - Invalid size: " << istr.str() << std::endl;
                return -1;
            }
            return -1;
        }
#endif
#ifdef XPUINFO_USE_RAPIDJSON
        if ((arg == "-write_json") && (a + 1 < argc))
        {
            testWriteJSON(argv[++a]);
        }
        else if (arg == "-verify_json")
        {
            testVerifyJSON();
        }
        else if ((arg == "-from_json") && (a + 1 < argc))
        {
            testReadJSON(argv[++a]);
        }
#endif
    }

    try
    {
        if (bRunTelemetry)
        {
            return runTelemetry(telemInterval_ms, telem_gpu_idx, peakOnly);
        }
        else 
        if (!testIndividual)
        {
            XI::Timer timer;
#ifdef _WIN32
            APIType apis = APIType((XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI \
                | XI::API_TYPE_DX11_INTEL_PERF_COUNTER | XI::API_TYPE_IGCL | XI::API_TYPE_OPENCL \
                | XI::API_TYPE_LEVELZERO \
                | XI::API_TYPE_DXCORE | XI::API_TYPE_NVML) | API_TYPE_WMI);
#else
            APIType apis = XI::API_TYPE_METAL;
#endif
            apis |= additionalAPIs;
            if (apiMask != XI::API_TYPE_UNKNOWN)
            {
                apis = apiMask;
            }
            timer.Start();
            std::cout << "Initializing XPUInfo with APIType = " << apis << "...\n";
            XI::XPUInfo xi(apis, runtimes);
            std::cout << xi << std::endl;
            timer.Stop();
            std::cout << "XPUInfo Time: " << timer.GetElapsedSecs() << " seconds\n";
        }
        else
        {
            std::vector<APIType> apiVec;
            apiVec.push_back(XPUINFO_INIT_ALL_APIS);  // All but WMI
            apiVec.push_back(APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI));

            // Either DXGI or DXCore is needed in all cases
            apiVec.push_back(APIType(XI::API_TYPE_DXGI | XI::API_TYPE_DX11_INTEL_PERF_COUNTER));
            apiVec.push_back(XI::API_TYPE_DXCORE);
            apiVec.push_back(APIType(XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI));
            apiVec.push_back(APIType(XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI | XI::API_TYPE_LEVELZERO));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_SETUPAPI));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_IGCL));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_OPENCL));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_LEVELZERO));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_NVML));

            for (auto api : apiVec)
            {
                std::cout << "Initializing XPUInfo with APIType = " << api << "...\n";
                XI::Timer timer;
                timer.Start();
                XI::XPUInfo xi(api, runtimes);
                std::cout << xi << std::endl;
                timer.Stop();
                std::cout << "XPUInfo Time: " << timer.GetElapsedSecs() << " seconds\n";
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception initializing XPUInfo: " << e.what() << std::endl;
        return -1;
    }
    catch (...)
    {
        std::cout << "Unknown exception initializing XPUInfo!\n";
        return -1;
    }
    return 0;
}
