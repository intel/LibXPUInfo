// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef XPUINFO_USE_RAPIDJSON
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h" // for XI::convert
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/istreamwrapper.h>
// Support serialization
#include <optional>

#define XPUINFO_JSON_VERSION "0.0.1"

namespace XI
{
namespace JSON
{
    template <typename T>
    const char* safeGetValString(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsString())
            {
                return val[valName].GetString();
            }
        }
        return nullptr;
    }

    template <typename T>
    std::string safeGetString(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsString())
            {
                return val[valName].GetString();
            }
        }
        return std::string();
    }

    template <typename T>
    std::wstring safeGetWString(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsString())
            {
                return convert(val[valName].GetString());
            }
        }
        return std::wstring();
    }

    template <typename T>
    std::optional<XI::UI64> safeGetUI64(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsUint64())
            {
                return val[valName].GetUint64();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<XI::UI64> safeGetI64(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsInt64())
            {
                return val[valName].GetInt64();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<XI::UI32> safeGetUI32(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsUint())
            {
                return val[valName].GetUint();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<XI::I32> safeGetI32(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsInt())
            {
                return val[valName].GetInt();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<double> safeGetDouble(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsDouble())
            {
                return val[valName].GetDouble();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<bool> safeGetBool(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsBool())
            {
                return val[valName].GetBool();
            }
        }
        return std::nullopt;
    }

    // For validating original vs. deserialized objects
    bool compareXI(const XPUInfoPtr& pXI, const XPUInfoPtr& pXID);

} // JSON

template <typename Alloc>
rapidjson::Value serializeDesc(const DXGI_ADAPTER_DESC1& desc, Alloc& a)
{
    rapidjson::Value outDesc(rapidjson::kObjectType);
    outDesc.AddMember("VendorID", desc.VendorId, a);
    outDesc.AddMember("DeviceID", desc.DeviceId, a);
    outDesc.AddMember("SubSysID", desc.SubSysId, a);
    outDesc.AddMember("Revision", desc.Revision, a);
    outDesc.AddMember("DedicatedVideoMemory", UI64(desc.DedicatedVideoMemory), a);
    outDesc.AddMember("DedicatedSystemMemory", UI64(desc.DedicatedSystemMemory), a);
    outDesc.AddMember("SharedSystemMemory", UI64(desc.SharedSystemMemory), a);
    outDesc.AddMember("AdapterLuid", reinterpretAsUI64(desc.AdapterLuid), a); // Not consistent between processes
    outDesc.AddMember("Flags", desc.Flags, a);
    return outDesc;
}

template <typename Alloc>
rapidjson::Value Device::serialize(Alloc& a)
{
    rapidjson::Value curDev(rapidjson::kObjectType);

    curDev.AddMember("Name", XI::convert(name()), a);
    curDev.AddMember("AdapterIndex", getAdapterIndex(), a);
    curDev.AddMember("DriverVersion", driverVersion().GetAsString(), a);
    curDev.AddMember("DriverVersionRaw", driverVersion().GetAsUI64(), a);
    auto pDI = getProperties().pDriverInfo;
    if (pDI)
    {
        rapidjson::Value curDI(rapidjson::kObjectType);
        curDI.AddMember("DriverDesc", XI::convert(pDI->DriverDesc), a);
        curDI.AddMember("DeviceDesc", XI::convert(pDI->DeviceDesc), a);
        curDI.AddMember("DriverVersion", XI::convert(pDI->DriverVersion), a);
        curDI.AddMember("DriverInfSection", XI::convert(pDI->DriverInfSection), a);
        curDI.AddMember("DeviceInstanceId", XI::convert(pDI->DeviceInstanceId), a);
        curDI.AddMember("LocationInfo", pDI->LocationInfo.serialize(a), a);
#ifdef _WIN32
        FILETIME ftime;
        ftime = pDI->DriverDate;
        curDI.AddMember("DriverDate", reinterpretAsUI64(ftime), a);
        curDI.AddMember("DriverDateString", Win::getDateString(pDI->DriverDate), a);
        ftime = pDI->InstallDate;
        curDI.AddMember("InstallDate", reinterpretAsUI64(ftime), a);
        curDI.AddMember("InstallDateString", Win::getDateString(pDI->InstallDate), a);
#endif
        curDev.AddMember("DriverInfo", curDI, a);
    }
    curDev.AddMember("Type", getType(), a);
    curDev.AddMember("DedicatedMemory", getProperties().DedicatedMemorySize, a);
    curDev.AddMember("SharedMemory", getProperties().SharedMemorySize, a);
    curDev.AddMember("MemoryBandWidthMax", getProperties().MemoryBandWidthMax, a);
    curDev.AddMember("FreqMaxMHz", getProperties().FreqMaxMHz, a);
    curDev.AddMember("FreqMinMHz", getProperties().FreqMinMHz, a);

    curDev.AddMember("dxgiDesc", serializeDesc(getProperties().dxgiDesc, a), a);

    curDev.AddMember("GenerationID", getProperties().DeviceGenerationID, a);
    curDev.AddMember("DeviceIPVersion", getProperties().DeviceIPVersion, a);
    auto genName = getProperties().getDeviceGenerationName();
    if (genName)
    {
        curDev.AddMember("GenerationName", std::string(genName), a);
    }
    curDev.AddMember("GenerationAPI", (XI::UI32)getProperties().DeviceGenerationAPI, a);
    curDev.AddMember("ComputeUnits", getProperties().NumComputeUnits, a);
    curDev.AddMember("ComputeUnitsSIMDWidth", getProperties().ComputeUnitSIMDWidth, a);
    curDev.AddMember("PackageTDP", getProperties().PackageTDP, a);

    curDev.AddMember("validAPIs", getCurrentAPIs(), a);
    curDev.AddMember("UMA", getProperties().UMA, a);
    curDev.AddMember("PCIAddress", getProperties().PCIAddress.serialize(a), a);

    curDev.AddMember("PCIDeviceGen", getProperties().PCIDeviceGen, a);
    curDev.AddMember("PCIDeviceWidth", getProperties().PCIDeviceWidth, a);
    curDev.AddMember("PCICurrentGen", getProperties().PCICurrentGen, a);
    curDev.AddMember("PCICurrentWidth", getProperties().PCICurrentWidth, a);

    curDev.AddMember("MediaFreqMaxMHz", getProperties().MediaFreqMaxMHz, a);
    curDev.AddMember("MediaFreqMinMHz", getProperties().MediaFreqMinMHz, a);
    curDev.AddMember("MemoryFreqMaxMHz", getProperties().MemoryFreqMaxMHz, a);
    curDev.AddMember("MemoryFreqMinMHz", getProperties().MemoryFreqMinMHz, a);

    curDev.AddMember("VendorFlags", getProperties().VendorFlags.IntelFeatureFlagsUI32, a);

    curDev.AddMember("IsHighPerformance", getProperties().IsHighPerformance, a);
    curDev.AddMember("IsMinimumPower", getProperties().IsMinimumPower, a);
    curDev.AddMember("IsDetachable", getProperties().IsDetachable, a);

    return curDev;
}

std::optional<DXGI_ADAPTER_DESC1> deserializeDesc(const rapidjson::Value& val, const char* devName);

template <typename rjDocOrVal, typename Alloc>
bool XPUInfo::serialize(rjDocOrVal& doc, Alloc& a)
{
    doc.AddMember("Version", XPUINFO_JSON_VERSION, a);
    doc.AddMember("UsedAPIsUI32", (UI32)m_UsedAPIs, a);

    rapidjson::Value valDevices(rapidjson::kArrayType);
    for (const auto& [luid, dev] : getDeviceMap())
    {
        auto curDev = dev->serialize(a);
        valDevices.PushBack(curDev, a);
    }
    doc.AddMember("Devices", valDevices, a);

    rapidjson::Value objCPU(rapidjson::kObjectType);
    const auto& CPU = getCPUDevice();
    auto pi = CPU.getProcInfo();
    if (pi)
    {
        objCPU.AddMember("Name", std::string(pi->brandString), a);
        objCPU.AddMember("VendorID", std::string(pi->vendorID), a);
        objCPU.AddMember("Cores", pi->numPhysicalCores, a);
        objCPU.AddMember("LogicalProcessors", pi->numLogicalCores, a);
        objCPU.AddMember("Hybrid", pi->hybrid, a);
        objCPU.AddMember("FeatureFlagsUI64", pi->flagsUI64, a);
        objCPU.AddMember("CPUID_1_EAX", pi->cpuid_1_eax, a);

        // TODO: Frequency
    }
    doc.AddMember("CPU", objCPU, a);

    // WML, DML, OV versions
#ifdef XPUINFO_USE_RUNTIMEVERSIONINFO
    const auto& verMap = getRuntimeVersionInfo();
    rapidjson::Value valVersionInfo(rapidjson::kArrayType);
    for (auto& vi : verMap)
    {
        rapidjson::Value valVI(rapidjson::kObjectType);
        valVI.AddMember(rapidjson::GenericStringRef(vi.first.c_str()), vi.second.getAsString(), a);
        valVI.AddMember("productVersion", vi.second.productVersion, a);
        valVersionInfo.PushBack(valVI, a);
    }
    doc.AddMember("RuntimeVersionInfo", valVersionInfo, a);
#endif

    auto pSysInfo = getSystemInfo();
    if (pSysInfo)
    {
        rapidjson::Value objSystem = pSysInfo->serialize(a);
        doc.AddMember("System", objSystem, a);
    }

#ifdef XPUINFO_USE_SYSTEMEMORYINFO
    if (m_pMemoryInfo)
    {
        rapidjson::Value objMem(rapidjson::kObjectType);
        objMem.AddMember("InstalledPhysicalMemory", (UI64)m_pMemoryInfo->getInstalledPhysicalMemory(), a);
        objMem.AddMember("TotalPhysicalMemory", (UI64)m_pMemoryInfo->getTotalPhysicalMemory(), a);
        objMem.AddMember("AvailablePhysicalMemoryAtInit", (UI64)m_pMemoryInfo->getAvailablePhysicalMemoryAtInit(), a);
        objMem.AddMember("PageSize", (UI64)m_pMemoryInfo->getPageSize(), a);
        doc.AddMember("Memory", objMem, a);
    }
#endif

    return true;
}

#ifdef XPUINFO_USE_SYSTEMEMORYINFO
template <typename rjDocOrVal>
SystemMemoryInfo::SystemMemoryInfo(const rjDocOrVal& val)/*: m_pSysInfo(nullptr)*/
{
    m_installedPhysicalMemory = JSON::safeGetUI64(val, "InstalledPhysicalMemory").value_or(0);
    m_totalPhysicalMemory = JSON::safeGetUI64(val, "TotalPhysicalMemory").value_or(0);
    m_availablePhysicalMemoryAtInit = JSON::safeGetUI64(val, "AvailablePhysicalMemoryAtInit").value_or(0);
    m_pageSize = JSON::safeGetUI64(val, "PageSize").value_or(0);
}
#endif

template <typename rjDocOrVal>
DeviceCPU::DeviceCPU(const rjDocOrVal& val): 
    DeviceBase(DeviceBase::kAdapterIndex_CPU, DEVICE_TYPE_CPU),
    m_initialMXCSR(UI32(-1)) // TODO
{
    m_pProcInfo.reset(new HybridDetect::PROCESSOR_INFO);
    auto name = JSON::safeGetValString(val, "Name");
    if (name)
    {
        strncpy(m_pProcInfo->brandString, name, sizeof(m_pProcInfo->brandString)-1);
    }
    else
    {
        strncpy(m_pProcInfo->brandString, "UNKNOWN", sizeof(m_pProcInfo->brandString)-1);
    }
    m_pProcInfo->brandString[sizeof(m_pProcInfo->brandString) - 1] = '\0'; // Force null-termination, CWE 170

    auto vendor = JSON::safeGetValString(val, "VendorID");
    if (vendor)
    {
        strncpy(m_pProcInfo->vendorID, vendor, sizeof(m_pProcInfo->vendorID)-1);
    }
    else
    {
        strncpy(m_pProcInfo->vendorID, "UNKNOWN", sizeof(m_pProcInfo->vendorID)-1);
    }
    m_pProcInfo->vendorID[sizeof(m_pProcInfo->vendorID) - 1] = '\0'; // Force null-termination, CWE 170

    m_pProcInfo->numPhysicalCores = JSON::safeGetUI32(val, "Cores").value_or(0);
    m_pProcInfo->numLogicalCores = JSON::safeGetUI32(val, "LogicalProcessors").value_or(0);
    m_pProcInfo->hybrid = JSON::safeGetBool(val, "Hybrid").value_or(false);
    m_pProcInfo->flagsUI64 = JSON::safeGetUI64(val, "FeatureFlagsUI64").value_or(0);
    m_pProcInfo->cpuid_1_eax = JSON::safeGetUI32(val, "CPUID_1_EAX").value_or(0);

}

template <typename rjDocOrVal>
XPUInfoPtr XPUInfo::deserialize(const rjDocOrVal& val)
{
    XPUInfoPtr xiPtr;

    std::string version = val["Version"].GetString();
    XPUINFO_REQUIRE(version == XPUINFO_JSON_VERSION);

    xiPtr.reset(new XPUInfo(XI::API_TYPE_DESERIALIZED));

    xiPtr->m_UsedAPIs = (XI::APIType)XI::JSON::safeGetUI32(val, "UsedAPIsUI32").value_or(0);

    if (val.HasMember("CPU"))
    {
        xiPtr->m_pCPU.reset(new DeviceCPU(val["CPU"]));
    }

    if (val.HasMember("Devices") && val["Devices"].IsArray())
    {
        auto& valDevs = val["Devices"];
        for (rapidjson::SizeType i = 0; i < valDevs.Size(); ++i)
        {
            auto devPtr = Device::deserialize(valDevs[i]);
            XPUINFO_REQUIRE_MSG(!!devPtr, "Device deserialize failed!");
            xiPtr->m_Devices.emplace(std::make_pair(devPtr->getLUID(), devPtr));
        }
    }

#ifdef XPUINFO_USE_SYSTEMEMORYINFO
    if (val.HasMember("Memory") && val["Memory"].IsObject())
    {
        xiPtr->m_pMemoryInfo.reset(new SystemMemoryInfo(val["Memory"]));
    }
#endif

    if (val.HasMember("System") && val["System"].IsObject())
    {
        xiPtr->m_pSystemInfo.reset(new SystemInfo(val["System"]));
    }

#ifdef XPUINFO_USE_RUNTIMEVERSIONINFO
    if (val.HasMember("RuntimeVersionInfo") && val["RuntimeVersionInfo"].IsArray())
    {
        auto& valRVI = val["RuntimeVersionInfo"];
        for (rapidjson::SizeType i = 0; i < valRVI.Size(); ++i)
        {
            xiPtr->m_RuntimeVersions.emplace(
                std::make_pair(valRVI[i].MemberBegin()->name.GetString(), 
                    RuntimeVersion(valRVI[i])));
        }
    }
#endif
    return xiPtr;
}

template <typename rjDocOrVal>
SystemInfo::SystemInfo(const rjDocOrVal& val)
{
    Manufacturer = JSON::safeGetWString(val, "Manufacturer");
    Model = JSON::safeGetWString(val, "Model");
    NumberOfLogicalProcessors = JSON::safeGetI32(val, "NumberOfLogicalProcessors").value_or(0);
    NumberOfProcessors = JSON::safeGetI32(val, "NumberOfProcessors").value_or(0);
    TotalPhysicalMemory = JSON::safeGetI64(val, "TotalPhysicalMemory").value_or(0);

    if (val.HasMember("OS") && val["OS"].IsObject())
    {
        OS.BuildNumber = JSON::safeGetWString(val["OS"], "BuildNumber");
        OS.Caption = JSON::safeGetWString(val["OS"], "Caption");
        OS.LastBootUpDate = JSON::safeGetWString(val["OS"], "LastBootUpDate");
        OS.LocalDate = JSON::safeGetWString(val["OS"], "LocalDate");
    }

    if (val.HasMember("BIOS") && val["BIOS"].IsObject())
    {
        BIOS.Name = JSON::safeGetWString(val["BIOS"], "Name");
        BIOS.Manufacturer = JSON::safeGetWString(val["BIOS"], "Manufacturer");
        BIOS.Version = JSON::safeGetWString(val["BIOS"], "Version");
        BIOS.ReleaseDate = JSON::safeGetWString(val["BIOS"], "ReleaseDate");
    }

    if (val.HasMember("m_mapMemSize") && val["m_mapMemSize"].IsArray())
    {
        auto& map = val["m_mapMemSize"];
        for (rapidjson::SizeType i = 0; i < map.Size(); ++i)
        {
            m_mapMemSize.emplace(std::make_pair(MemoryDeviceInfo{
                JSON::safeGetUI32(map[i], "SpeedMHz").value_or(0),
                JSON::safeGetUI64(map[i], "Capacity").value_or(0),
                }, 
                JSON::safeGetUI32(map[i], "Count").value_or(0))
                );
        }
    }
}

template <typename Alloc>
rapidjson::Value SystemInfo::serialize(Alloc& a) const
{
    rapidjson::Value curSI(rapidjson::kObjectType);

    curSI.AddMember("Manufacturer", convert(Manufacturer), a);
    curSI.AddMember("Model", convert(Model), a);
    curSI.AddMember("NumberOfLogicalProcessors", NumberOfLogicalProcessors, a);
    curSI.AddMember("NumberOfProcessors", NumberOfProcessors, a);
    //
    curSI.AddMember("TotalPhysicalMemory", TotalPhysicalMemory, a);

    rapidjson::Value curOS(rapidjson::kObjectType);
    curOS.AddMember("BuildNumber", convert(OS.BuildNumber), a);
    curOS.AddMember("Caption", convert(OS.Caption), a);
    curOS.AddMember("CodeSet", convert(OS.CodeSet), a);
    curOS.AddMember("CountryCode", convert(OS.CountryCode), a);
    curOS.AddMember("Name", convert(OS.Name), a);
    //
    curOS.AddMember("LastBootUpDate", convert(OS.LastBootUpDate), a);
    curOS.AddMember("LocalDate", convert(OS.LocalDate), a);
    curSI.AddMember("OS", curOS, a);

    rapidjson::Value curBIOS(rapidjson::kObjectType);
    curBIOS.AddMember("Name", convert(BIOS.Name), a);
    curBIOS.AddMember("Manufacturer", convert(BIOS.Manufacturer), a);
    curBIOS.AddMember("Version", convert(BIOS.Version), a);
    curBIOS.AddMember("ReleaseDate", convert(BIOS.ReleaseDate), a);
    curSI.AddMember("BIOS", curBIOS, a);

    rapidjson::Value memDevInfoArr(rapidjson::kArrayType);
    for (const auto [speedCap, count] : m_mapMemSize)
    {
        rapidjson::Value mdi(rapidjson::kObjectType);
        mdi.AddMember("SpeedMHz", speedCap.SpeedMHz, a);
        mdi.AddMember("Capacity", speedCap.Capacity, a);
        mdi.AddMember("Count", count, a);
        memDevInfoArr.PushBack(mdi, a);
    }
    curSI.AddMember("m_mapMemSize", memDevInfoArr, a);

    return curSI;
}

template <typename rjDocOrVal>
PCIAddressType::PCIAddressType(const rjDocOrVal& val) // deserialize
{
    domain = val["domain"].GetUint();
    bus = val["bus"].GetUint();
    device = val["device"].GetUint();
    function = val["function"].GetUint();
}

template <typename Alloc>
rapidjson::Value PCIAddressType::serialize(Alloc& a) const
{
    rapidjson::Value curLoc(rapidjson::kObjectType);
    curLoc.AddMember("domain", domain, a);
    curLoc.AddMember("bus", bus, a);
    curLoc.AddMember("device", device, a);
    curLoc.AddMember("function", function, a);
    return curLoc;
}

} // XI
#endif // XPUINFO_USE_RAPIDJSON
