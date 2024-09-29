// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_OPENCL
#include "LibXPUInfo.h"
#include "CL/opencl.hpp"
#include "DebugStream.h"

#ifndef CL_DEVICE_IP_VERSION_INTEL
/* For GPU devices, version 1.0.0: */

#define CL_DEVICE_IP_VERSION_INTEL                0x4250
#define CL_DEVICE_ID_INTEL                        0x4251
#define CL_DEVICE_NUM_SLICES_INTEL                0x4252
#define CL_DEVICE_NUM_SUB_SLICES_PER_SLICE_INTEL  0x4253
#define CL_DEVICE_NUM_EUS_PER_SUB_SLICE_INTEL     0x4254
#define CL_DEVICE_NUM_THREADS_PER_EU_INTEL        0x4255
#define CL_DEVICE_FEATURE_CAPABILITIES_INTEL      0x4256

typedef cl_bitfield         cl_device_feature_capabilities_intel;

/* For GPU devices, version 1.0.0: */

#define CL_DEVICE_FEATURE_FLAG_DP4A_INTEL         (1 << 0)
#define CL_DEVICE_FEATURE_FLAG_DPAS_INTEL         (1 << 1)
#endif

namespace XI
{

void Device::initOpenCLDevice(cl_platform_id inPlatform, cl_device_id inDevice, const std::string& inExtensions)
{
	m_CLPlatform = inPlatform;
	m_CLDevice = inDevice;

	cl::Device clDevice(inDevice);

	cl_int err;
	cl::string devName;
	devName = clDevice.getInfo<CL_DEVICE_NAME>(&err);
	HYBRIDDETECT_DEBUG_REQUIRE(CL_SUCCESS == err);
	m_OpenCLAdapterName = std::move(devName);

	if (m_props.NumComputeUnits == -1)
	{
		cl_uint maxCU = 0;
		err = clDevice.getInfo<cl_uint>(CL_DEVICE_MAX_COMPUTE_UNITS, &maxCU);
		if (CL_SUCCESS == err)
			m_props.NumComputeUnits = (I32)maxCU;
	}

	if (m_props.FreqMaxMHz == -1)
	{
		cl_uint maxFreq = 0;
		err = clDevice.getInfo<cl_uint>(CL_DEVICE_MAX_CLOCK_FREQUENCY, &maxFreq);
		if (CL_SUCCESS == err)
			m_props.FreqMaxMHz = (I32)maxFreq;
	}

	if (inExtensions.find("cl_intel_device_attribute_query") != std::string::npos)
	{
		cl_version ip_version = 0;
		err = clDevice.getInfo<cl_version>(CL_DEVICE_IP_VERSION_INTEL, &ip_version);
		if ((CL_SUCCESS==err) && ip_version)
		{
			if (m_props.DeviceGenerationAPI == API_TYPE_UNKNOWN)
			{
				m_props.DeviceGenerationAPI = API_TYPE_OPENCL;
				m_props.DeviceGenerationID = ip_version;
			}
			if (m_props.DeviceIPVersion == 0)
			{
				m_props.DeviceIPVersion = ip_version;
			}
		}

		cl_device_feature_capabilities_intel features = 0;
		err = clDevice.getInfo<cl_device_feature_capabilities_intel>(CL_DEVICE_FEATURE_CAPABILITIES_INTEL, &features);
		if ((CL_SUCCESS == err) && features)
		{
			if (features & CL_DEVICE_FEATURE_FLAG_DP4A_INTEL)
			{
				m_props.VendorFlags.IntelFeatureFlags.FLAG_DP4A = 1;
			}
			if (features & CL_DEVICE_FEATURE_FLAG_DPAS_INTEL)
			{
				m_props.VendorFlags.IntelFeatureFlags.FLAG_DPAS = 1;
			}
		}
	}

	{ // IGCL may be wrong with old drivers, so allow CL to fix it
		cl_bool isUMA = 0;
		err = clDevice.getInfo<cl_bool>(CL_DEVICE_HOST_UNIFIED_MEMORY, &isUMA);
		if (CL_SUCCESS == err)
		{
			if ((m_props.UMA == UMA_UNKNOWN) || (isUMA && (m_props.UMA == NONUMA_DISCRETE)) ||
				(!isUMA && (m_props.UMA == UMA_INTEGRATED)))
			{
				m_props.UMA = isUMA ? UMA_INTEGRATED : NONUMA_DISCRETE;
			}
		}
	}
	validAPIs = validAPIs | API_TYPE_OPENCL;
}

void XPUInfo::initOpenCL()
{
	std::vector<cl::Platform> platforms;
	cl_int err = cl::Platform::get(&platforms);
	if (CL_SUCCESS == err)
	{
		int clDevsFound = 0;
		for (auto& platform : platforms)
		{
			cl::string pfVendor, pfName;
			pfVendor = platform.getInfo<CL_PLATFORM_VENDOR>(&err);
			HYBRIDDETECT_DEBUG_REQUIRE(CL_SUCCESS == err);
			pfName = platform.getInfo<CL_PLATFORM_NAME>(&err);
			HYBRIDDETECT_DEBUG_REQUIRE(CL_SUCCESS == err);

			DebugStream dStr(false);
			dStr << "Platform vendor = " << pfVendor << " \tname = " << pfName << std::endl;
			if (pfVendor == "Microsoft")
			{
				dStr << "Skipping platform!" << std::endl;
				continue;
			}

			std::vector<cl::Device> clDevices;
			err = platform.getDevices(CL_DEVICE_TYPE_GPU, &clDevices);
			if (CL_SUCCESS == err)
			{
				for (auto& clDevice : clDevices)
				{
					cl::string devName;
					devName = clDevice.getInfo<CL_DEVICE_NAME>(&err);
					HYBRIDDETECT_DEBUG_REQUIRE(CL_SUCCESS == err);
					dStr << "\t" << devName;

					// 			if (HasExtension())
					cl::string exts = clDevice.getInfo<CL_DEVICE_EXTENSIONS>(&err);
					HYBRIDDETECT_DEBUG_REQUIRE(CL_SUCCESS == err);
					cl_bool luidFromCLValid = CL_FALSE;
					UI64 luidFromCL = 0;
					bool curDevFound = false;
					if (exts.find("cl_khr_device_uuid") != cl::string::npos)
					{
						err = clDevice.getInfo<cl_bool>(CL_DEVICE_LUID_VALID_KHR, &luidFromCLValid);
						HYBRIDDETECT_DEBUG_REQUIRE(CL_SUCCESS == err);
						err = clDevice.getInfo<UI64>(CL_DEVICE_LUID_KHR, &luidFromCL);
						HYBRIDDETECT_DEBUG_REQUIRE(CL_SUCCESS == err);
						if (luidFromCLValid)
						{
							dStr << ", LUID = " << std::hex << luidFromCL;

							auto xiDev = getDeviceInternal(luidFromCL);
							if (xiDev)
							{
								curDevFound = true;
								xiDev->initOpenCLDevice(platform(), clDevice(), exts);
								dStr << ", CL Platform = " << xiDev->m_CLPlatform << ", CL Device = " << xiDev->m_CLDevice;
								++clDevsFound;
							}

							dStr << std::dec;
						}
					}
					if (!curDevFound)
					{
						auto xiDev = getDeviceInternal(devName.c_str());
						if (xiDev)
						{
							curDevFound = true;
							xiDev->initOpenCLDevice(platform(), clDevice(), exts);
							++clDevsFound;
						}
					}
					dStr << std::endl;
				}
			}
		}
		if (clDevsFound)
		{
			m_UsedAPIs = m_UsedAPIs | API_TYPE_OPENCL;
		}
	}
}

} // XI
#endif // LIBXPUINFO_USE_OPENCL
