// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_RAPIDJSON

#include "LibXPUInfo_JSON.h"
#include "LibXPUInfo_Util.h"
#include <wchar.h>

namespace XI
{
    using namespace JSON;
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

            newDev->m_props.MemoryBandWidthMax = safeGetI64(val, "MemoryBandWidthMax").value_or(-1);

            return newDev;
        }
        return nullptr;
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
