// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_RAPIDJSON

#include "LibXPUInfo_JSON.h"
#include "LibXPUInfo_Util.h"
#include <wchar.h>

namespace XI
{
using namespace JSON;

static rapidjson::Value serializeMapToJson(rapidjson::Document& doc, const std::map<unsigned, std::vector<ULONG>>& inputMap) {
    using namespace rapidjson;
    auto& allocator = doc.GetAllocator();

    // Create a JSON object for the map
    Value mapObject(kObjectType);

    // Iterate through the map
    for (const auto& pair : inputMap) {
        // Convert unsigned key to string (JSON keys must be strings)
        std::string keyStr = std::to_string(pair.first);

        // Create a JSON array for the vector
        Value jsonArray(kArrayType);
        for (const auto& value : pair.second) {
            jsonArray.PushBack((std::uint32_t)value, allocator); // Add ULONG to array
        }

        // Add key-value pair to map object
        Value key(keyStr.c_str(), allocator); // Create JSON string key
        mapObject.AddMember(key, jsonArray, allocator);
    }

    return mapObject;
}

static rapidjson::Value serializeCoreMasksToJson(rapidjson::Document& doc, const std::map<short, ULONG64>& coreMasks) {
    using namespace rapidjson;
    auto& allocator = doc.GetAllocator();

    // Create a JSON object for the map
    Value mapObject(kObjectType);

    // Iterate through the map
    for (const auto& pair : coreMasks) {
        // Convert short key to string (JSON keys must be strings)
        std::string keyStr = std::to_string(pair.first);

        // Create a JSON value for ULONG64
        Value jsonValue;
        jsonValue.SetUint64(pair.second); // Use SetUint64 for ULONG64

        // Add key-value pair to map object
        Value key(keyStr.c_str(), allocator); // Create JSON string key
        mapObject.AddMember(key, jsonValue, allocator);
    }

    return mapObject;
}


bool XPUInfo::serialize(rapidjson::Document& doc)
{
    auto& a = doc.GetAllocator();
    //std::result_of<decltype(&rapidjson::Document::GetAllocator)(rapidjson::Document)>::type& a;

    doc.AddMember("Version", XPUINFO_JSON_VERSION, a);
    doc.AddMember("APIVersion", XPUINFO_API_VERSION, a);
    doc.AddMember("ClientBuildTimestamp", m_clientBuildTimestamp, a);
    doc.AddMember("InternalBuildTimestamp", m_internalBuildTimestamp, a);
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

        // cpuSets: std::map<unsigned, std::vector<ULONG>>
        objCPU.AddMember("cpuSets", serializeMapToJson(doc, pi->cpuSets), a);
        objCPU.AddMember("coreMasks", serializeCoreMasksToJson(doc, pi->coreMasks), a);

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

XPUInfoPtr XPUInfo::deserialize(const rapidjson::Document& val)
{
    XPUInfoPtr xiPtr;

    // Note: Version field is not used (yet)
    //std::string version = val["Version"].GetString();
    //XPUINFO_REQUIRE(version == XPUINFO_JSON_VERSION);
    String clientBuildTimestamp("Unknown");
    if (val.HasMember("ClientBuildTimestamp"))
    {
        clientBuildTimestamp = val["ClientBuildTimestamp"].GetString();
    }
    xiPtr = std::make_unique<XPUInfo>(XI::API_TYPE_DESERIALIZED, XI::RuntimeNames(), sizeof(XPUInfo), clientBuildTimestamp.c_str());

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

std::optional<DXGI_ADAPTER_DESC1> deserializeDesc(const rapidjson::Value& val, const char* devName)
{
    using namespace XI::JSON;
    if (val.HasMember("dxgiDesc"))
    {
        auto& valDesc = val["dxgiDesc"];

        DXGI_ADAPTER_DESC1 desc{};

        std::wstring descW = XI::convert(devName);
        wcsncpy(desc.Description, descW.c_str(), 128);
        desc.VendorId = safeGetUI32(valDesc, "VendorID").value_or(-1);
        desc.DeviceId = safeGetUI32(valDesc, "DeviceID").value_or(-1);
        desc.SubSysId = safeGetUI32(valDesc, "SubSysID").value_or(-1);
        desc.Revision = safeGetUI32(valDesc, "Revision").value_or(-1);
        desc.DedicatedVideoMemory = safeGetUI64(valDesc, "DedicatedVideoMemory").value_or(-1);
        desc.DedicatedSystemMemory = safeGetUI64(valDesc, "DedicatedSystemMemory").value_or(-1);
        desc.SharedSystemMemory = safeGetUI64(valDesc, "SharedSystemMemory").value_or(-1);
        auto luid = safeGetUI64(valDesc, "AdapterLuid");
        *(UI64*)&desc.AdapterLuid = luid.value_or(UI64(-1));
        desc.Flags = safeGetUI32(valDesc, "Flags").value_or(-1);

        return desc;
    }
    return std::nullopt;
}

DevicePtr Device::deserialize(const rapidjson::Value& val)
{
    // name, index, desc, type, api
    auto name = safeGetValString(val, "Name");
    auto index = safeGetUI32(val, "AdapterIndex");
    auto desc = deserializeDesc(val, name);
    auto type = safeGetUI32(val, "Type");
    auto apis = safeGetUI32(val, "validAPIs");
    auto rawDriverVersion = safeGetUI64(val, "DriverVersionRaw");
    if (index.has_value() && desc.has_value() && type.has_value() && apis.has_value()
        && rawDriverVersion.has_value())
    {
        DevicePtr newDev(new Device(index.value(), &desc.value(), DeviceType(type.value()),
            APIType(apis.value() | API_TYPE_DESERIALIZED), rawDriverVersion.value()));
                
        if (val.HasMember("DriverInfo"))
        {
            auto& valDI = val["DriverInfo"];
            newDev->m_props.pDriverInfo.reset(new DriverInfo);
#ifdef _WIN32
            auto DriverDate = safeGetUI64(valDI, "DriverDate");
            auto InstallDate = safeGetUI64(valDI, "InstallDate");
            if (DriverDate.has_value() && InstallDate.has_value())
            {
                newDev->m_props.pDriverInfo->DriverDate = *reinterpret_cast<FILETIME*>(&DriverDate.value());
                newDev->m_props.pDriverInfo->InstallDate = *reinterpret_cast<FILETIME*>(&InstallDate.value());
            }
#endif
            newDev->m_props.pDriverInfo->DriverDesc = safeGetWString(valDI, "DriverDesc");
            newDev->m_props.pDriverInfo->DeviceDesc = safeGetWString(valDI, "DeviceDesc");
            newDev->m_props.pDriverInfo->DriverVersion = safeGetWString(valDI, "DriverVersion");
            newDev->m_props.pDriverInfo->DriverInfSection = safeGetWString(valDI, "DriverInfSection");
            newDev->m_props.pDriverInfo->DeviceInstanceId = safeGetWString(valDI, "DeviceInstanceId");

            if (valDI.HasMember("LocationInfo"))
            {
                newDev->m_props.pDriverInfo->LocationInfo = XI::PCIAddressType(valDI["LocationInfo"]);
            }
        }
        newDev->m_props.FreqMaxMHz = safeGetI32(val, "FreqMaxMHz").value_or(-1);
        newDev->m_props.FreqMinMHz = safeGetI32(val, "FreqMinMHz").value_or(-1);
        newDev->m_props.DeviceGenerationID = safeGetI32(val, "GenerationID").value_or(-1);
        newDev->m_props.DeviceIPVersion = safeGetUI32(val, "DeviceIPVersion").value_or(0);
        newDev->m_props.DeviceGenerationAPI = APIType(safeGetUI32(val, "GenerationAPI").value_or(-1));
        newDev->m_props.NumComputeUnits = safeGetI32(val, "ComputeUnits").value_or(-1);
        newDev->m_props.ComputeUnitSIMDWidth = safeGetI32(val, "ComputeUnitsSIMDWidth").value_or(-1);
        newDev->m_props.PackageTDP = safeGetI32(val, "PackageTDP").value_or(-1);
        newDev->m_props.UMA = UMAType(safeGetUI32(val, "UMA").value_or(0));
        if (val.HasMember("PCIAddress")) {
            newDev->m_props.PCIAddress = PCIAddressType(val["PCIAddress"]);
        }
            
        newDev->m_props.PCICurrentGen = safeGetI32(val, "PCICurrentGen").value_or(-1);
        newDev->m_props.PCICurrentWidth = safeGetI32(val, "PCICurrentWidth").value_or(-1);
        newDev->m_props.PCIDeviceGen = safeGetI32(val, "PCIDeviceGen").value_or(-1);
        newDev->m_props.PCIDeviceWidth = safeGetI32(val, "PCIDeviceWidth").value_or(-1);
        newDev->m_props.VendorFlags.IntelFeatureFlagsUI32 = safeGetUI32(val, "VendorFlags").value_or(0);
        if (newDev->IsVendor(kVendorId_nVidia))
        {
            newDev->m_props.VendorSpecific.nVidia.cudaComputeCapability_Major = 
                safeGetI32(val, "cudaComputeCapability_Major").value_or(-1);
            newDev->m_props.VendorSpecific.nVidia.cudaComputeCapability_Minor =
                safeGetI32(val, "cudaComputeCapability_Minor").value_or(-1);
        }
        newDev->m_props.MemoryBandWidthMax = safeGetI64(val, "MemoryBandWidthMax").value_or(-1);

        return newDev;
    }
    return nullptr;
}

rapidjson::Value serializeDesc(const DXGI_ADAPTER_DESC1& desc, AllocatorType& a)
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

rapidjson::Value Device::serialize(AllocatorType& a)
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
    if (IsVendor(kVendorId_nVidia))
    {
        curDev.AddMember("cudaComputeCapability_Major", 
            getProperties().VendorSpecific.nVidia.cudaComputeCapability_Major, a);
        curDev.AddMember("cudaComputeCapability_Minor",
            getProperties().VendorSpecific.nVidia.cudaComputeCapability_Minor, a);
    }

    curDev.AddMember("IsHighPerformance", getProperties().IsHighPerformance, a);
    curDev.AddMember("IsMinimumPower", getProperties().IsMinimumPower, a);
    curDev.AddMember("IsDetachable", getProperties().IsDetachable, a);

    return curDev;
}

#ifdef XPUINFO_USE_SYSTEMEMORYINFO
SystemMemoryInfo::SystemMemoryInfo(const rapidjson::Value& val)/*: m_pSysInfo(nullptr)*/
{
    m_installedPhysicalMemory = JSON::safeGetUI64(val, "InstalledPhysicalMemory").value_or(0);
    m_totalPhysicalMemory = JSON::safeGetUI64(val, "TotalPhysicalMemory").value_or(0);
    m_availablePhysicalMemoryAtInit = JSON::safeGetUI64(val, "AvailablePhysicalMemoryAtInit").value_or(0);
    m_pageSize = JSON::safeGetUI64(val, "PageSize").value_or(0);
}
#endif

static void deserializeMapFromJson(std::map<unsigned, std::vector<ULONG>> &result, const rapidjson::Value& mapObject) {
    using namespace rapidjson;

    // Iterate through the map object
    for (Value::ConstMemberIterator itr = mapObject.MemberBegin(); itr != mapObject.MemberEnd(); ++itr) {
        // Get the key and convert to unsigned
        const char* keyStr = itr->name.GetString();
        unsigned key;
        try {
            key = std::stoul(keyStr);
        }
        catch (const std::exception&) {
            throw std::runtime_error("Invalid map key (must be unsigned integer): " + std::string(keyStr));
        }

        // Get the array
        if (!itr->value.IsArray()) {
            throw std::runtime_error("Value for key " + std::string(keyStr) + " must be an array");
        }

        // Convert array to vector<ULONG>
        std::vector<ULONG> values;
        for (const auto& val : itr->value.GetArray()) {
            if (!val.IsUint64()) {
                throw std::runtime_error("Array element for key " + std::string(keyStr) + " must be an unsigned integer");
            }
            values.push_back(static_cast<ULONG>(val.GetUint64()));
        }

        // Add to result map
        result[key] = values;
    }
}

static void deserializeCoreMasksFromJson(std::map<short, ULONG64>& result, const rapidjson::Value& mapObject) {
    using namespace rapidjson;

    // Iterate through the map object
    for (Value::ConstMemberIterator itr = mapObject.MemberBegin(); itr != mapObject.MemberEnd(); ++itr) {
        // Get the key and convert to short
        const char* keyStr = itr->name.GetString();
        short key;
        try {
            int keyInt = std::stoi(keyStr); // Use stoi to handle negative numbers
            if (keyInt < std::numeric_limits<short>::min() || keyInt > std::numeric_limits<short>::max()) {
                throw std::out_of_range("Key out of range for short");
            }
            key = static_cast<short>(keyInt);
        }
        catch (const std::exception&) {
            throw std::runtime_error("Invalid map key (must be short integer): " + std::string(keyStr));
        }

        // Get the value and convert to ULONG64
        if (!itr->value.IsUint64()) {
            throw std::runtime_error("Value for key " + std::string(keyStr) + " must be an unsigned 64-bit integer");
        }
        ULONG64 value = itr->value.GetUint64();

        // Add to result map
        result[key] = value;
    }
}

DeviceCPU::DeviceCPU(const rapidjson::Value& val) :
    DeviceBase(DeviceBase::kAdapterIndex_CPU, DEVICE_TYPE_CPU),
    m_initialMXCSR(UI32(-1)) // TODO
{
    m_pProcInfo.reset(new HybridDetect::PROCESSOR_INFO);
    auto name = JSON::safeGetValString(val, "Name");
    if (name)
    {
        strncpy(m_pProcInfo->brandString, name, sizeof(m_pProcInfo->brandString) - 1);
    }
    else
    {
        strncpy(m_pProcInfo->brandString, "UNKNOWN", sizeof(m_pProcInfo->brandString) - 1);
    }
    m_pProcInfo->brandString[sizeof(m_pProcInfo->brandString) - 1] = '\0'; // Force null-termination, CWE 170

    auto vendor = JSON::safeGetValString(val, "VendorID");
    if (vendor)
    {
        strncpy(m_pProcInfo->vendorID, vendor, sizeof(m_pProcInfo->vendorID) - 1);
    }
    else
    {
        strncpy(m_pProcInfo->vendorID, "UNKNOWN", sizeof(m_pProcInfo->vendorID) - 1);
    }
    m_pProcInfo->vendorID[sizeof(m_pProcInfo->vendorID) - 1] = '\0'; // Force null-termination, CWE 170

    m_pProcInfo->numPhysicalCores = JSON::safeGetUI32(val, "Cores").value_or(0);
    m_pProcInfo->numLogicalCores = JSON::safeGetUI32(val, "LogicalProcessors").value_or(0);
    m_pProcInfo->hybrid = JSON::safeGetBool(val, "Hybrid").value_or(false);
    m_pProcInfo->flagsUI64 = JSON::safeGetUI64(val, "FeatureFlagsUI64").value_or(0);
    m_pProcInfo->cpuid_1_eax = JSON::safeGetUI32(val, "CPUID_1_EAX").value_or(0);

    if (val.HasMember("cpuSets") && val["cpuSets"].IsObject())
    {
        deserializeMapFromJson(m_pProcInfo->cpuSets, val["cpuSets"]);
    }

    if (val.HasMember("coreMasks") && val["coreMasks"].IsObject())
    {
        deserializeCoreMasksFromJson(m_pProcInfo->coreMasks, val["coreMasks"]);
    }
}

SystemInfo::SystemInfo(const rapidjson::Value& val)
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

rapidjson::Value SystemInfo::serialize(JSON::AllocatorType& a) const
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

PCIAddressType::PCIAddressType(const rapidjson::Value& val) // deserialize
{
    domain = val["domain"].GetUint();
    bus = val["bus"].GetUint();
    device = val["device"].GetUint();
    function = val["function"].GetUint();
}

rapidjson::Value PCIAddressType::serialize(JSON::AllocatorType& a) const
{
    rapidjson::Value curLoc(rapidjson::kObjectType);
    curLoc.AddMember("domain", domain, a);
    curLoc.AddMember("bus", bus, a);
    curLoc.AddMember("device", device, a);
    curLoc.AddMember("function", function, a);
    return curLoc;
}

// These operators are for use by compareXI - only built with JSON for size optimization
static
bool operator==(const DXGI_ADAPTER_DESC1& l, const DXGI_ADAPTER_DESC1& r)
{
	// Not comparing LUID
	bool eq = (
		(wcscmp(l.Description, r.Description)==0)
		&& (l.VendorId == r.VendorId)
		&& (l.DeviceId == r.DeviceId)
		&& (l.SubSysId == r.SubSysId)
		&& (l.Revision == r.Revision)
		&& (l.DedicatedVideoMemory == r.DedicatedVideoMemory)
		&& (l.DedicatedSystemMemory == r.DedicatedSystemMemory)
		&& (l.SharedSystemMemory == r.SharedSystemMemory)
		&& (l.Flags == r.Flags)
		);
	XPUINFO_REQUIRE_MSG(eq, "DXGI_ADAPTER_DESC1 mismatch");
	return eq;
}

static
bool operator==(const DriverInfo& l, const DriverInfo& r)
{
	// Not comparing LUID
	bool eq = ((l.DriverDesc == r.DriverDesc)
		&& (l.DeviceDesc == r.DeviceDesc)
		&& (l.DriverVersion == r.DriverVersion)
		&& (l.DriverInfSection == r.DriverInfSection)
		&& (l.DeviceInstanceId == r.DeviceInstanceId)
		&& (l.LocationInfo == r.LocationInfo)
#ifdef _WIN32
		&& (reinterpretAsUI64(l.DriverDate) == reinterpretAsUI64(r.DriverDate))
		&& (reinterpretAsUI64(l.InstallDate) == reinterpretAsUI64(r.InstallDate))
#endif
		);
	XPUINFO_REQUIRE_MSG(eq, "DriverInfo mismatch");
	return eq;
}

bool DeviceProperties::operator==(const DeviceProperties& props) const
{
	if ((dxgiDesc == props.dxgiDesc)
		&& (DedicatedMemorySize == props.DedicatedMemorySize)
		&& (SharedMemorySize == props.SharedMemorySize)
		&& (MemoryBandWidthMax == props.MemoryBandWidthMax)
		&& (PCIDeviceGen == props.PCIDeviceGen)
		&& (PCIDeviceWidth == props.PCIDeviceWidth)
		&& (PCICurrentGen == props.PCICurrentGen)
		&& (PCICurrentWidth == props.PCICurrentWidth)
		//
		&& (PCIAddress == props.PCIAddress)
		&& (UMA == props.UMA)
		&& (DeviceGenerationID == props.DeviceGenerationID)
		//
		&& (DeviceGenerationAPI == props.DeviceGenerationAPI)
		&& (NumComputeUnits == props.NumComputeUnits)
		&& (ComputeUnitSIMDWidth == props.ComputeUnitSIMDWidth)
		&& (PackageTDP == props.PackageTDP)
		//
		&& (VendorFlags.IntelFeatureFlagsUI32 == props.VendorFlags.IntelFeatureFlagsUI32)
		//
		)
	{
		if (pDriverInfo)
		{
			return (props.pDriverInfo
				&& (*pDriverInfo == *props.pDriverInfo)
				);
		}
		return true;
	}
	XPUINFO_REQUIRE_MSG(false, "DeviceProperties mismatch");
	return false;
}

bool Device::operator==(const Device& dev) const
{
	if ((m_type == dev.m_type)
		&& (m_adapterIndex == dev.m_adapterIndex)
		&& (validAPIs == (dev.validAPIs & ~API_TYPE_DESERIALIZED))
		&& (m_pDriverVersion && dev.m_pDriverVersion && 
			(m_pDriverVersion->GetAsUI64()==dev.m_pDriverVersion->GetAsUI64()))
		&& (m_props == dev.m_props)
		)
	{
		return true;
	}
	XPUINFO_REQUIRE_MSG(false, __FUNCTION__);
	return false;
}

#ifdef XPUINFO_USE_RUNTIMEVERSIONINFO
RuntimeVersion::RuntimeVersion(const rapidjson::Value& val)
{
    std::istringstream ver(val.MemberBegin()->value.GetString());
    ver >> major;
    char c;
    ver >> c;
    ver >> minor;
    ver >> c;
    ver >> build;
    XPUINFO_REQUIRE(!ver.bad());
    productVersion = JSON::safeGetString(val, "productVersion");
}
#endif

namespace JSON
{
    bool compareXI(const XPUInfoPtr& pXI, const XPUInfoPtr& pXID)
    {
        auto refAPIs = pXI->getUsedAPIs();
        auto desAPIs = pXID->getUsedAPIs() & ~XI::API_TYPE_DESERIALIZED;
        if (refAPIs != desAPIs)
        {
            XPUINFO_REQUIRE_MSG(false, "API type mismatch");
            return false;
        }
        if (pXI->getDeviceMap().size() != pXID->getDeviceMap().size())
        {
            XPUINFO_REQUIRE_MSG(false, "Device count mismatch");
            return false;
        }
        bool devsMatch = true;
        for (const auto& pDev : pXI->getDeviceMap())
        {
            const auto& pDevD = pXID->getDeviceByIndex(pDev.second->getAdapterIndex());
            devsMatch = *pDev.second == *pDevD;
            if (!devsMatch)
            {
                XPUINFO_REQUIRE_MSG(false, "Device mismatch: " << convert(pDev.second->name()));
                break;
            }
        }
        return devsMatch;
    }
} // JSON
} // XI
#endif // XPUINFO_USE_RAPIDJSON
