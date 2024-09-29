// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_LEVELZERO
#include "LibXPUInfo.h"
#include "ze_api.h"
#include "zes_api.h"
#include "DebugStream.h"
#include "LibXPUInfo_Util.h"

#ifndef _DEBUG
#define XPUINFO_L0_VERBOSE false
#else
#define XPUINFO_L0_VERBOSE true
#endif

namespace
{
// ** May need to remove these when updating L0 runtime headers **

///////////////////////////////////////////////////////////////////////////////
/// @brief Supported Dot Product flags
typedef uint32_t ze_intel_device_module_dp_exp_flags_t;
typedef enum _ze_intel_device_module_dp_exp_flag_t {
    ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A = ZE_BIT(0), ///< Supports DP4A operation
    ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS = ZE_BIT(1), ///< Supports DPAS operation
    ZE_INTEL_DEVICE_MODULE_EXP_FLAG_FORCE_UINT32 = 0x7fffffff

} ze_intel_device_module_dp_exp_flag_t;

///////////////////////////////////////////////////////////////////////////////
#define ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES (ze_structure_type_t)0x00030013
///////////////////////////////////////////////////////////////////////////////
/// @brief Device Module dot product properties queried using
///        ::zeDeviceGetModuleProperties
///
/// @details
///     - This structure may be passed to ::zeDeviceGetModuleProperties, via
///       `pNext` member of ::ze_device_module_properties_t.
/// @brief Device module dot product properties
typedef struct _ze_intel_device_module_dp_exp_properties_t {
    ze_structure_type_t stype = ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES; ///< [in] type of this structure
    void *pNext;                                                                    ///< [in,out][optional] must be null or a pointer to an extension-specific
                                                                                    ///< structure (i.e. contains sType and pNext).
    ze_intel_device_module_dp_exp_flags_t flags;                                    ///< [out] 0 (none) or a valid combination of ::ze_intel_device_module_dp_flag_t
} ze_intel_device_module_dp_exp_properties_t;

}

namespace XI
{

L0_Extensions::L0_Extensions(size_t inSize) : std::vector<ze_driver_extension_properties_t>(inSize)
{
}

const ze_driver_extension_properties_t* L0_Extensions::find(const char* inExtName) const
{
	for (const auto& e : *this)
	{
		if (strcmp(e.name, inExtName) == 0)
		{
			return &e;
		}
	}
	return nullptr;
}

void Device::initL0Device(ze_device_handle_t inL0Device, const ze_device_properties_t& device_properties, const L0_Extensions& exts)
{
	if (inL0Device)
	{
		m_L0Device = inL0Device;
		validAPIs = validAPIs | API_TYPE_LEVELZERO;

		updateIfDstNotSet(m_props.ComputeUnitSIMDWidth, (I32)device_properties.physicalEUSimdWidth);
		I32 numEUs = device_properties.numSlices * device_properties.numSubslicesPerSlice * device_properties.numEUsPerSubslice;
		updateIfDstNotSet(m_props.NumComputeUnits, numEUs);
		updateIfDstNotSet(m_props.FreqMaxMHz, (I32)device_properties.coreClockRate);

		// zeDeviceGetModuleProperties, see https://github.com/intel/compute-runtime/blob/fb838afe42185d7270fcfb57383187f2b601cc2a/level_zero/doc/experimental_extensions/MODULE_DP_SUPPORT.md?plain=1#L66
		if (exts.find("ZE_intel_experimental_device_module_dp_properties"))
		{
			ze_device_module_properties_t deviceModProps = { ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES };
			ze_intel_device_module_dp_exp_properties_t moduleDpProps = { ZE_STRUCTURE_INTEL_DEVICE_MODULE_DP_EXP_PROPERTIES };
			deviceModProps.pNext = &moduleDpProps;

			ze_result_t zRet = zeDeviceGetModuleProperties(m_L0Device, &deviceModProps);
			if (ZE_RESULT_SUCCESS == zRet)
			{
				if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DP4A) {
					m_props.VendorFlags.IntelFeatureFlags.FLAG_DP4A = 1;
				}
				else if (deviceModProps.flags & ZE_DEVICE_MODULE_FLAG_DP4A)
				{
					m_props.VendorFlags.IntelFeatureFlags.FLAG_DP4A = 1;
				}

				if (moduleDpProps.flags & ZE_INTEL_DEVICE_MODULE_EXP_FLAG_DPAS) {
					m_props.VendorFlags.IntelFeatureFlags.FLAG_DPAS = 1;
				}
			}
		}

		if (exts.find("ZE_extension_device_ip_version"))
		{
			ze_device_ip_version_ext_t* pProp = (ze_device_ip_version_ext_t*)device_properties.pNext;
			// Traverse linked list until end or found ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT
			while (pProp && 
				((ze_device_ip_version_ext_t*)pProp)->stype != ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT)
			{
				pProp = ((ze_device_ip_version_ext_t*)pProp->pNext);
			}
			if (pProp && pProp->ipVersion)
			{
				if (m_props.DeviceGenerationAPI == API_TYPE_UNKNOWN)
				{
					m_props.DeviceGenerationAPI = API_TYPE_LEVELZERO;
					m_props.DeviceGenerationID = pProp->ipVersion;
				}
				if (m_props.DeviceIPVersion == 0)
				{
					m_props.DeviceIPVersion = pProp->ipVersion;
				}
			}
		}

		zes_pci_properties_t pci_props{ ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
		ze_result_t zRet = zesDevicePciGetProperties(m_L0Device, &pci_props);
		if (ZE_RESULT_SUCCESS == zRet)
		{
			DebugStreamW dStr(XPUINFO_L0_VERBOSE);
			if (pci_props.haveBandwidthCounters)
				dStr << L"L0 Device " << name() << " has bandwidth counters!\n";
			if (!m_props.PCIAddress.valid() && isValidPCIAddr(pci_props.address))
			{
				m_props.PCIAddress.domain = pci_props.address.domain;
				m_props.PCIAddress.bus = pci_props.address.bus;
				m_props.PCIAddress.device = pci_props.address.device;
				m_props.PCIAddress.function = pci_props.address.function;
			}
		}

		uint32_t domain_count = 0;
		ze_result_t zRes = zesDeviceEnumFrequencyDomains(inL0Device, &domain_count, nullptr);

		if ((ZE_RESULT_SUCCESS == zRes) && (domain_count > 0))
		{
			std::vector<zes_freq_handle_t> freqHandlesL0(domain_count);
			zRes = zesDeviceEnumFrequencyDomains(
				inL0Device, &domain_count, freqHandlesL0.data());
			XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);

			for (uint32_t i = 0; i < freqHandlesL0.size(); ++i)
			{
				zes_freq_properties_t domain_props{
					ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
				zRes = zesFrequencyGetProperties(freqHandlesL0[i], &domain_props);
				if ((ZE_RESULT_SUCCESS == zRes))
				{
					if (domain_props.type == ZES_FREQ_DOMAIN_GPU)
					{
						updateIfDstNotSet(m_props.FreqMaxMHz, I32(domain_props.max));
						updateIfDstNotSet(m_props.FreqMinMHz, I32(domain_props.min));
					}
					else if (domain_props.type == ZES_FREQ_DOMAIN_MEDIA)
					{
						updateIfDstNotSet(m_props.MediaFreqMaxMHz, I32(domain_props.max));
						updateIfDstNotSet(m_props.MediaFreqMinMHz, I32(domain_props.min));
					}
					else if (domain_props.type == ZES_FREQ_DOMAIN_MEMORY)
					{
						updateIfDstNotSet(m_props.MemoryFreqMaxMHz, I32(domain_props.max));
						updateIfDstNotSet(m_props.MemoryFreqMinMHz, I32(domain_props.min));
					}
				}
			}
		}

		zes_pwr_handle_t pwrHandle;
		zRet = zesDeviceGetCardPowerDomain(m_L0Device, &pwrHandle);
		if (ZE_RESULT_SUCCESS == zRet) // Only on DG2 so far
		{
			uint32_t limitsCount = 0;
			zRet = zesPowerGetLimitsExt(pwrHandle, &limitsCount, nullptr);
			if (ZE_RESULT_SUCCESS == zRet)
			{
				std::vector<zes_power_limit_ext_desc_t> pwrLimitExtDescs(limitsCount);
				zRet = zesPowerGetLimitsExt(pwrHandle, &limitsCount, pwrLimitExtDescs.data());
				if (ZE_RESULT_SUCCESS == zRet)
				{
					for (const auto& pl : pwrLimitExtDescs)
					{
						if (pl.limit && (pl.limitUnit == ZES_LIMIT_UNIT_POWER) && (pl.level == ZES_POWER_LEVEL_SUSTAINED))
						{
							updateIfDstNotSet(m_props.PackageTDP, pl.limit / 1000);
							break;
						}
					}
				}
			}
		}
	
#if 0 // TODO: Try zesDeviceEventRegister/zesDriverEventListen.  Returns not supported on DG2/UHD 770.
		zes_event_type_flags_t registerFlags =
			//ZES_EVENT_TYPE_FLAG_DEVICE_DETACH | ZES_EVENT_TYPE_FLAG_DEVICE_ATTACH |
			//ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER | ZES_EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_EXIT |
			//ZES_EVENT_TYPE_FLAG_FREQ_THROTTLED |
			//ZES_EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED | 
			//ZES_EVENT_TYPE_FLAG_TEMP_CRITICAL |
			//ZES_EVENT_TYPE_FLAG_TEMP_THRESHOLD1 | ZES_EVENT_TYPE_FLAG_TEMP_THRESHOLD2 |
			ZES_EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED
			;
		zRes = zesDeviceEventRegister(inL0Device, registerFlags);
		XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);
#endif
#if 0 // RAS seems to be unsupported
		uint32_t numRAS = 0;
		zRes = zesDeviceEnumRasErrorSets(inL0Device, &numRAS, nullptr);
		XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);
		if (numRAS >= 1) // None for DG2 or UHD 770
		{
			std::vector<zes_ras_handle_t> rasHandles(numRAS);
			zRes = zesDeviceEnumRasErrorSets(inL0Device, &numRAS, rasHandles.data());
			XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);

#if 0
			// Query for number of error categories supported by platform
			uint32_t rasCategoryCount = 0;
			zesRasGetStateExp(rasHandle, &rasCategoryCount, nullptr);

			zes_ras_state_exp_t* rasStates = (zes_ras_state_exp_t*)allocate(rasCategoryCount * sizeof(zes_ras_state_exp_t));

			//Gather error states
			{s}RasGetStateExp(rasHandle, &rasCategoryCount, rasStates);

			// Print error details
			for (uint32_t i = 0; i < rasCategoryCount; i++) {
				output(" Error category: %d, Error count: %llun n", rasStates[i]->category, rasStates[i]->errorCounter);
			}

			// Clear error counter for specific category, for example PROGRAMMING_ERRORS
			{s}RasClearStateExp(rasHandle, ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS);
#endif
		}
#endif

#if 0 // Done in IGCL - skip this
		zes_pci_state_t pci_state{ ZES_STRUCTURE_TYPE_PCI_STATE, };
		//zes_pci_stats_t
		zRes = zesDevicePciGetState(m_L0Device, &pci_state);
		if (ZE_RESULT_SUCCESS == zRes)
		{
			// See https://hsdes.intel.com/appstore/article/#/14018523690
			if (pci_state.speed.maxBandwidth != UI64(-1))
			{
				DebugStream dStr(XPUINFO_L0_VERBOSE);
				dStr << "NOTE: zesDevicePciGetState returned maxBandwidth = " << pci_state.speed.maxBandwidth / double(1024 * 1024 * 1024) << std::endl;
				//bUpdate = true;
			}
		}
#endif

#if 0
		// Try zesDeviceEnumPowerDomains, zesDeviceGetCardPowerDomain
		uint32_t numPowerDomains = 0;
		zes_pwr_handle_t pwrHandle;
		zRet = zesDeviceGetCardPowerDomain(m_L0Device, &pwrHandle);
		if (ZE_RESULT_SUCCESS == zRet) // Only on DG2 so far
		{
			zes_power_ext_properties_t pwrPropsExt{ ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES };
			zes_power_properties_t pwrProps{ ZES_STRUCTURE_TYPE_POWER_PROPERTIES };
			//pwrProps.pNext = &pwrPropsExt; // unsupported
			zRet = zesPowerGetProperties(pwrHandle, &pwrProps); // TODO: not working on DG2
			if (ZE_RESULT_SUCCESS == zRet)
			{
				if (pwrProps.maxLimit)
				{
					printf("1\n");
				}
			}

			uint32_t limitsCount = 0;
			zRet = zesPowerGetLimitsExt(pwrHandle, &limitsCount, nullptr);
			if (ZE_RESULT_SUCCESS == zRet)
			{
				std::vector<zes_power_limit_ext_desc_t> pwrLimitExtDescs(limitsCount);
				zRet = zesPowerGetLimitsExt(pwrHandle, &limitsCount, pwrLimitExtDescs.data());
				if (ZE_RESULT_SUCCESS == zRet)
				{
					for (const auto& pl : pwrLimitExtDescs)
					{
						if (pl.limit && (pl.limitUnit == ZES_LIMIT_UNIT_POWER) && (pl.level == ZES_POWER_LEVEL_SUSTAINED))
						{
							std::cout << pl.limit/1000.0 << " Watts (sustained)" << std::endl;
							updateIfDstNotSet(m_props.PackageTDP, pl.limit / 1000);
						}
					}
				}
			}
		}

		zRet = zesDeviceEnumPowerDomains(m_L0Device, &numPowerDomains, nullptr);
		if (ZE_RESULT_SUCCESS == zRet)
		{
			std::vector<zes_pwr_handle_t> powerHandles(numPowerDomains);
			zRet = zesDeviceEnumPowerDomains(m_L0Device, &numPowerDomains, powerHandles.data());
			for (int i = 0; i < numPowerDomains; ++i)
			{
				zes_power_ext_properties_t pwrPropsExt{ ZES_STRUCTURE_TYPE_POWER_EXT_PROPERTIES };
				zes_power_properties_t pwrProps{ ZES_STRUCTURE_TYPE_POWER_PROPERTIES };
				//pwrProps.pNext = &pwrPropsExt;
				zRet = zesPowerGetProperties(powerHandles[i], &pwrProps); // TODO: not working
				if (ZE_RESULT_SUCCESS == zRet)
				{
					if (pwrProps.maxLimit)
					{
						printf("1\n");
					}
				}
			}
		}
		uint32_t num_oc_domains = 0;
		zRet = zesDeviceEnumOverclockDomains(m_L0Device, &num_oc_domains, nullptr);
		if (ZE_RESULT_SUCCESS == zRet)
		{
			printf("1\n");
		}
		// zesOverclockGetDomainControlProperties
		// zesPowerGetLimitsExt, zesPowerGetProperties
#endif
#if 0 //def _DEBUG
		//     ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES = 0x1000e,   ///< ::ze_device_memory_ext_properties_t
		ze_device_memory_ext_properties_t device_memory_ext_properties = {};
		device_memory_ext_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_EXT_PROPERTIES;
		UI32 numMemProps = 0;
		auto zRes = zeDeviceGetMemoryProperties(inL0Device, &numMemProps, nullptr);
		if ((ZE_RESULT_SUCCESS == zRes) && numMemProps)
		{
			std::vector<ze_device_memory_properties_t> memProps(numMemProps);
			for (auto& p : memProps)
			{
				p.stype = ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES;
			}
			memProps.begin()->pNext = &device_memory_ext_properties;
			zRes = zeDeviceGetMemoryProperties(inL0Device, &numMemProps, memProps.data());
			XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);
		}
		ze_pci_ext_properties_t pci_ext_props = {};
		pci_ext_props.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;
		zRes = zeDevicePciGetPropertiesExt(inL0Device, &pci_ext_props);
		XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);
		// So far, we can get GPU memory size and type (LP5, GDDR6, etc.)
#endif
			//if (ZE_DEVICE_TYPE_GPU == device_properties.type)
	}
}

void XPUInfo::initL0()
{
	std::vector<L0Enum> L0Drivers; // temp

	ze_result_t result;
	// Initialize the driver
    // TODO: Once a loader is available supporting zeInitDrivers(), use it instead of zeInit. 
	//       See https://spec.oneapi.io/level-zero/latest/core/api.html#zeinitdrivers.
    // NOTE: As of NPU driver 100.2761, the NPU only enumerates when zeInit flags include
	//       ZE_INIT_FLAG_VPU_ONLY. If zeInit(0) is called earlier in the process lifetime, 
	//       then all subsequent calls to zeInit will not enumerate the NPU. 
	//       One side-effect is that the OpenVINO NPU plugin also fails to enumerate the NPU. 
	//       If this happens unexpectedly, check your app for calls to zeInit, or other 
	//       modules using it such as IGCL.
	result = zeInit(ZE_INIT_FLAG_VPU_ONLY|ZE_INIT_FLAG_GPU_ONLY); // We must use both flags to get both GPU and NPU, or else OpenVINO will fail to see NPU!
	if (result != ZE_RESULT_SUCCESS)
	{
		DebugStream dStr(XPUINFO_L0_VERBOSE);
		dStr << "Driver not initialized: " << std::to_string(result) << std::endl;
	}
	else
	{
		//std::cout << "Driver initialized.\n";
		ze_result_t status;
		uint32_t driverCount = 0;
		status = zeDriverGet(&driverCount, nullptr);
		if (status != ZE_RESULT_SUCCESS) {
			DebugStream dStr(XPUINFO_L0_VERBOSE);
			dStr << "zeDriverGet Failed with return code: " << std::to_string(status) << std::endl;
			return;
		}
		//std::cout << "zeDriverGet: Found " << driverCount << " drivers\n";

		std::vector<ze_driver_handle_t> drivers(driverCount);
		status = zeDriverGet(&driverCount, drivers.data());

		if (status != ZE_RESULT_SUCCESS) {
			DebugStream dStr(XPUINFO_L0_VERBOSE);
			dStr << "zeDriverGet Failed with return code: " << std::to_string(status) << std::endl;
			return;
		}

		L0Drivers.resize(driverCount);
		for (uint32_t driver = 0; driver < driverCount; ++driver)
		{
			ze_driver_handle_t pDriver = drivers[driver];
			L0Drivers[driver].driver = pDriver;

			// get all devices
			uint32_t deviceCount = 0;
			status = zeDeviceGet(pDriver, &deviceCount, nullptr);
			XPUINFO_DEBUG_REQUIRE(status == ZE_RESULT_SUCCESS);
			//std::cout << "zeDeviceGet: Found " << deviceCount << " devices\n";
			L0Drivers[driver].devices.resize(deviceCount);

			status = zeDeviceGet(pDriver, &deviceCount, L0Drivers[driver].devices.data());
			XPUINFO_DEBUG_REQUIRE(status == ZE_RESULT_SUCCESS);
		}
	}

	for (auto l0enum : L0Drivers)
	{
		ze_result_t zRes;
		uint32_t numExts = 0;
		zRes = zeDriverGetExtensionProperties(l0enum.driver, &numExts, nullptr);
		L0_Extensions driverExts(numExts);
		const ze_driver_extension_properties_t* pLuidExt = nullptr;
		const ze_driver_extension_properties_t* pDevIPExt = nullptr;
		if (ZE_RESULT_SUCCESS == zRes)
		{
			zRes = zeDriverGetExtensionProperties(l0enum.driver, &numExts, driverExts.data());
			XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);
			// See https://spec.oneapi.io/level-zero/latest/core/EXT.html
			pLuidExt = driverExts.find("ZE_extension_device_luid");
			pDevIPExt = driverExts.find("ZE_extension_device_ip_version");
			// ZE_extension_eu_count
		}

		for (auto l0device : l0enum.devices)
		{
			ze_device_properties_t device_properties = {};
			device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
			ze_device_ip_version_ext_t zeDeviceIpVersion = { ZE_STRUCTURE_TYPE_DEVICE_IP_VERSION_EXT };
			ze_device_luid_ext_properties_t device_luid_ext_properties = { ZE_STRUCTURE_TYPE_DEVICE_LUID_EXT_PROPERTIES };
			if (pLuidExt)
			{
				device_properties.pNext = &device_luid_ext_properties;
				if (pDevIPExt)
				{
					device_luid_ext_properties.pNext = &zeDeviceIpVersion;
				}
			}
			else if (pDevIPExt)
			{
				device_properties.pNext = &zeDeviceIpVersion;
			}

			zRes = zeDeviceGetProperties(l0device, &device_properties);
			XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);

			if (ZE_RESULT_SUCCESS == zRes)
			{
				// See https://hsdes.intel.com/appstore/article/#/14018468731 - [DG2][ADL] Level Zero device_luid_ext_properties_t always giving NULL LUID
				UI64 l0luid = LuidToUI64(&device_luid_ext_properties.luid.id[0]);
				bool bFound = false;
				if (pLuidExt && l0luid)
				{
					auto it = m_Devices.find(l0luid);
					if (it != m_Devices.end())
					{
						bFound = true;
						it->second->initL0Device(l0device, device_properties, driverExts);
					}
				}
				else
				{
					DebugStream dStr(XPUINFO_L0_VERBOSE);
					if (pLuidExt)
					{
						dStr << "ERROR: L0 LUID = 0 for device: " << device_properties.name << std::endl;
					}
					else
					{
						dStr << "Warning: L0 ZE_extension_device_luid not supported for device: " << device_properties.name << std::endl;
					}

					// Match by name
					UI32 maxIdx = 0;
					for (auto& dev : m_Devices)
					{
						maxIdx = std::max(maxIdx, dev.second->getAdapterIndex());
						if (strcmp(device_properties.name, convert(dev.second->name()).c_str()) == 0)
						{
							dev.second->initL0Device(l0device, device_properties, driverExts);
							bFound = true;
							break;
						}
						else if (dev.second->m_OpenCLAdapterName.size() &&
								(strcmp(device_properties.name, dev.second->m_OpenCLAdapterName.c_str()) == 0))
						{
							dev.second->initL0Device(l0device, device_properties, driverExts);
							bFound = true;
							break;
						}
					}
				}
				if (!bFound)
				{
					DebugStream dStr(XPUINFO_L0_VERBOSE);
					dStr << "ERROR: L0 device not initialized, no match found!" << std::endl;
				}
				else if ((m_UsedAPIs & API_TYPE_LEVELZERO) == 0)
				{
					m_UsedAPIs = m_UsedAPIs | API_TYPE_LEVELZERO;
				}
			}
			else
			{
				DebugStream dStr(XPUINFO_L0_VERBOSE);
				dStr << "ERROR: zeDeviceGetProperties returned " << zRes << std::endl;
			}
		}
	}
}

#define L0_TRACK_FREQUENCY_MEMORY 0 // In IGCL
void TelemetryTracker::InitL0()
{
	auto l0device = m_Device->getHandle_L0();
	if (l0device)
	{
		uint32_t domain_count = 0;
		ze_result_t zRes = zesDeviceEnumFrequencyDomains(l0device, &domain_count, nullptr);

		if ((ZE_RESULT_SUCCESS == zRes) && (domain_count > 0))
		{
			m_freqHandlesL0.resize(domain_count);
			zRes = zesDeviceEnumFrequencyDomains(
				l0device, &domain_count, m_freqHandlesL0.data());
			XPUINFO_DEBUG_REQUIRE(ZE_RESULT_SUCCESS == zRes);

			for (uint32_t i = 0; i < m_freqHandlesL0.size(); ++i) 
			{
				zes_freq_properties_t domain_props{
					ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
				zRes = zesFrequencyGetProperties(m_freqHandlesL0[i], &domain_props);
				if ((ZE_RESULT_SUCCESS == zRes))
				{
					if (domain_props.type == ZES_FREQ_DOMAIN_GPU)
					{
						//std::cout << "L0 GPU" << std::endl;
						if (!(m_Device->getCurrentAPIs() & API_TYPE_IGCL) && m_freqHandlesL0.size())
						{
							m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_FREQUENCY);
						}
					}
					else if (domain_props.type == ZES_FREQ_DOMAIN_MEDIA)
					{
						//std::cout << "L0 MEDIA" << std::endl;
						m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_FREQUENCY_MEDIA);
					}
#if L0_TRACK_FREQUENCY_MEMORY
					else if (domain_props.type == ZES_FREQ_DOMAIN_MEMORY)
					{
						m_ResultMask = (TelemetryItem)(m_ResultMask | TELEMETRYITEM_FREQUENCY_MEMORY);
					}
#endif
				}
			}
		}
	}
}

bool TelemetryTracker::RecordL0(TimedRecord& rec)
{
	bool bUpdate = false;

	// Get Freq from L0 if no IGCL
    if (!(m_Device->getCurrentAPIs() & API_TYPE_IGCL) && m_freqHandlesL0.size())
    {
        for (uint32_t i = 0; i < m_freqHandlesL0.size(); ++i) {
            zes_freq_properties_t domain_props{
                ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
            auto zRes = zesFrequencyGetProperties(m_freqHandlesL0[i], &domain_props);

            if ((ZE_RESULT_SUCCESS==zRes) && (domain_props.type == ZES_FREQ_DOMAIN_GPU))
            {
                zes_freq_state_t state{ ZES_STRUCTURE_TYPE_FREQ_STATE, };
                TimerTick tt = Timer::GetNow();
                zRes = zesFrequencyGetState(m_freqHandlesL0[i], &state);
				if (ZE_RESULT_SUCCESS == zRes)
				{
					rec.timeStamp = (double)(
#if XPUINFO_USE_STD_CHRONO
						tt.time_since_epoch()
#else
						tt
#endif
						/ Timer::GetScale() // TODO: optimize
						);
					rec.freq = state.actual;
					bUpdate = true;

					if (m_records.size() == 0)
					{
						m_startTime = rec.timeStamp;
					}
				}
				break;
            }
        }
    }

	for (uint32_t i = 0; i < m_freqHandlesL0.size(); ++i) {
		zes_freq_properties_t domain_props{
			ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
		auto zRes = zesFrequencyGetProperties(m_freqHandlesL0[i], &domain_props);

		if ((ZE_RESULT_SUCCESS == zRes) &&
			((domain_props.type == ZES_FREQ_DOMAIN_MEDIA)
#if L0_TRACK_FREQUENCY_MEMORY
				|| (domain_props.type == ZES_FREQ_DOMAIN_MEMORY)
#endif
				)
			)
		{
			zes_freq_state_t state{ ZES_STRUCTURE_TYPE_FREQ_STATE, };
			zRes = zesFrequencyGetState(m_freqHandlesL0[i], &state);
			if (ZE_RESULT_SUCCESS == zRes)
			{
				// TODO: Expose state.throttleReasons
				if (domain_props.type == ZES_FREQ_DOMAIN_MEDIA)
				{
					rec.freq_media = state.actual;
				}
#if L0_TRACK_FREQUENCY_MEMORY
				else
				{
					rec.freq_memory = state.actual;
				}
#endif
				bUpdate = true;

				if (m_records.size() == 0)
				{
					m_startTime = rec.timeStamp;
				}
			}
			break;
		}
	}

#if 0
	{
		auto hL0 = m_Device.getHandle_L0();

		zes_pci_properties_t pci_props{ ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
		ze_result_t zRes = zesDevicePciGetProperties(hL0, &pci_props);
		zes_pci_stats_t pci_stats{};
		if (ZE_RESULT_SUCCESS == zRes)
		{
			// See https://hsdes.intel.com/appstore/article/#/14018523837
			zRes = zesDevicePciGetStats(hL0, &pci_stats); // getting ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, support not planned for DG2
			DebugStream dStr(XPUINFO_L0_VERBOSE);
			if (ZE_RESULT_ERROR_UNSUPPORTED_FEATURE == zRes)
			{
				dStr << "zesDevicePciGetStats returned ZE_RESULT_ERROR_UNSUPPORTED_FEATURE" << std::endl;
			}
			else //if (ZE_RESULT_SUCCESS != zRes)
			{
				dStr << "zesDevicePciGetStats returned " << zRes << std::endl;
			}
		}
	}
#endif

	return bUpdate;
}

} // XI
#endif // XPUINFO_USE_LEVELZERO
