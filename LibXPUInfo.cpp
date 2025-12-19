// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#ifdef _WIN32
#include <d3d11_4.h>
#include <wrl/client.h>
#pragma comment(lib, "RuntimeObject.lib")

#include "DebugStream.h"
#include <psapi.h>
#endif // _WIN32

#include <sstream>
#include <exception>
#include <iomanip>
#include <unordered_map>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/vm_statistics.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/task.h>
#endif

#if defined(__linux__)
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

#if defined(_WIN32) && !defined(_M_ARM64)
namespace WRL = Microsoft::WRL;
#endif

namespace XI
{
	const char* get_filename_from_path(const char* path) 
	{
		const char* last_slash = strrchr(path, '/');
		const char* last_backslash = strrchr(path, '\\');
		const char* last_separator = last_slash > last_backslash ? last_slash : last_backslash;
		return last_separator ? last_separator + 1 : path;
	}

	void ErrorHandlerDefault(const std::string& message, const char* fileName, const int lineNumber)
	{
		std::ostringstream err;
		err << message << " at " << fileName << ":" << lineNumber;
		throw std::logic_error(err.str().c_str());
	}

	ErrorHandlerType g_ErrorHandlerFunc = ErrorHandlerDefault;

	ErrorHandlerType getErrorHandlerFunc()
	{
		return g_ErrorHandlerFunc;
	}

	ErrorHandlerType setErrorHandlerFunc(ErrorHandlerType f)
	{
		ErrorHandlerType tmp = g_ErrorHandlerFunc;
		g_ErrorHandlerFunc = f;
		return tmp;
	}

	bool XPUInfo::hasDXCore()
	{
#ifdef XPUINFO_USE_DXCORE
		// NOTE: We can catch delay-load failure in CreateDXCore(), so this could be removed by refactoring.  It works, though.
		// These dlls must be delay-loaded: nvml.dll;dxcore.dll;ext-ms-win-dxcore-l1-1-0.dll
		// To verify set of delay-loaded dlls, use this on oldest supported OS: https://github.com/lucasg/Dependencies
		static bool bHasDXCore = false;
		static bool bHasDXCoreInitialized = false;
		static std::mutex HasDXCoreMutex;

		if (bHasDXCoreInitialized)
		{
			return bHasDXCore;
		}

		{
			std::lock_guard<std::mutex> lock(HasDXCoreMutex);
			// dxcore.dll should be delay-loaded
			// Test by module existing rather than windows version test
			HMODULE hDxCore = LoadLibraryA("dxcore.dll");
			bool rval = false;
			if (hDxCore)
			{
				bHasDXCore = true;
				bHasDXCoreInitialized = true;
				rval = true;
				FreeLibrary(hDxCore);
			}
			return rval;
		}
#else
		return false;
#endif
	}

	// See https://github.com/oneapi-src/oneDNN/blob/0bbadfe56184c197e2b343f821deab6199f310dd/src/gpu/intel/jit/ngen/npack/neo_packager.hpp#L275
	struct ipvParts
	{
		UI32 revision : 6;
		UI32 reserved : 8;
		UI32 release : 8;
		UI32 architecture : 10;
	};
	union ipvUnion
	{
		UI32 ipVersion = 0; // From OpenCL, L0, or IGCL
		ipvParts ipv;
	};
	struct GenName {
		UI32 gen; // From Intel Device Information
		const char* name; 
		const char* infName=nullptr; // Part before first '_'
		ipvUnion ipvu;
	};

#define MAKE_FAMILY_NAME_PAIR(x) {IntelGfxFamily::i##x, #x}
	static const std::unordered_map<IntelGfxFamily, std::string> S_IntelGfxFamilyNameMap {
		MAKE_FAMILY_NAME_PAIR(Gen9_Generic),
		MAKE_FAMILY_NAME_PAIR(Gen11_Generic),
		MAKE_FAMILY_NAME_PAIR(Gen12LP_Generic),
		MAKE_FAMILY_NAME_PAIR(Gen12HP_DG2),
		MAKE_FAMILY_NAME_PAIR(Xe_S),
		MAKE_FAMILY_NAME_PAIR(Xe_L_MeteorLakeH),
		MAKE_FAMILY_NAME_PAIR(Xe_L_ArrowLakeH),
		MAKE_FAMILY_NAME_PAIR(Xe2_Generic),
		MAKE_FAMILY_NAME_PAIR(Xe2_LunarLake),
		MAKE_FAMILY_NAME_PAIR(Xe2_BattleMage),
		MAKE_FAMILY_NAME_PAIR(Xe3_Generic)
	};

	IntelGfxFamily getIntelGfxFamily(ipvParts ipv)
	{
		IntelGfxFamily outFamily = IntelGfxFamily::iUnknown;
		switch (ipv.architecture)
		{
		case 9:  outFamily = IntelGfxFamily::iGen9_Generic; break;
		case 11: outFamily = IntelGfxFamily::iGen11_Generic; break;
		case 12:
			outFamily = IntelGfxFamily::iGen12LP_Generic;
			if (ipv.release > 50 && ipv.release <= 59)
				outFamily = IntelGfxFamily::iGen12HP_DG2;
			else if (ipv.release == 70) // MTL-U, ARL-S, ARL-U
				outFamily = IntelGfxFamily::iXe_S;
			else if (ipv.release == 71)
				outFamily = IntelGfxFamily::iXe_L_MeteorLakeH;
			else if (ipv.release == 74)
				outFamily = IntelGfxFamily::iXe_L_ArrowLakeH;
			break;
		case 20: outFamily = IntelGfxFamily::iXe2_Generic; break;
		case 30: outFamily = IntelGfxFamily::iXe3_Generic; break;
		default: outFamily = IntelGfxFamily::iUnknown; break;
		}
		return outFamily;
	}

#if XPUINFO_HAS_CPP17
	std::optional<IntelGfxFamilyNamePair> Device::getIntelGfxFamilyName() const
	{
		if (IsVendor(kVendorId_Intel) && getType()==DEVICE_TYPE_GPU)
		{
			ipvUnion ipvu;
			ipvu.ipVersion = m_props.DeviceIPVersion;
			if (ipvu.ipVersion)
			{
				auto ipFamily = getIntelGfxFamily(ipvu.ipv);
				auto ipfIter = S_IntelGfxFamilyNameMap.find(ipFamily);
				if (ipfIter != S_IntelGfxFamilyNameMap.end())
				{
					return *ipfIter;
				}
			}
		}
		return std::nullopt;
	}
#endif

	/* This table is purposefully internal to LibXPUInfo. 
	*  Design goal is to expose information without creating end-user dependency.
	*  NOTE: So far, the value of "gen" increases with newer generations, but not always with "ipVersion",
	*		 so it is up to the user to do valid comparisons.
	*  TODO: Create option to override internal table with text input.
	*/
	static const GenName S_GenNameMap[] = 
	{
		{ 0x0e, "Haswell" }, { 0x10, "Broadwell" }, { 0x12, "Sky Lake" }, { 0x13, "Kaby Lake"}, {0x14, "Coffee Lake"},
		{0x1d, "Ice Lake"}, 
		{0x21, "Tiger Lake", "iTGLD",  0x3000000},
		{0x23, "Rocket Lake", "iRKLD", 0x3004000},
		{0x24, "Raptor Lake S", "iRPLSD", 0x3008000}, {0x24, "Alder Lake S", "iADLSD", 0x3008000}, // Same gen value
		{0x25, "Raptor Lake P", "iRPLPD", 0x3008000}, {0x25, "Alder Lake P", "iADLPD", 0x3008000}, // Same gen value
		{1210, "DG1"}, 
		{1270, "DG2", "iDG2D", 0x30dc008},
		{1272, "Meteor Lake", "iMTL", 0x311c004},
		{1272, "Meteor Lake", "MTL_IAG", 0x311c004}, // Inf name first seen with 101.5445
		{1273, "Arrow Lake",  "iARL", 0x3118004},
		{1274, "Battlemage", "BMG_", 0x5004000},
		{1275, "Lunar Lake", "iLNL", 0x5010001},
		{1275, "Lunar Lake", "LNL_", 0x5010001}, // TODO: Remove one of these when no longer needed
		{1275, "Lunar Lake", "LNL_", 0x5010004}, // TODO: Remove one of these when no longer needed
		{1300, "Panther Lake", "PTL_", 0x07800004},
		// Devices with no "Intel Device Information" value have negative values
		{0x80000000, "NPU2.7", "mtl_w" },
		{0x80000000, "NPU2.7", "NPU2_7" },
		{0x80000002, "NPU4", "NPU4" },
		{0x80000003, "NPU5", "NPU5" },
	};
	static const int S_numGenNames = sizeof(S_GenNameMap)/sizeof(GenName);

#if 0 // Not used
	// Could be used as member of DeviceProperties, but relying on this creates an end-user dependency on having table updated
	UI32 getIDIGenFromIPVersion(const UI32 IPVersion)
	{
		for (int i = 0; i < S_numGenNames; ++i)
		{
			if (S_GenNameMap[i].ipVersion == IPVersion)
			{
				return S_GenNameMap[i].gen;
			}
		}
		return 0;
	}
#endif

	static const std::unordered_map<XI::UI32, XI::String> S_nVArchNames =
	{
		{2, "Kepler"},
		{3, "Maxwell"},
		{4, "Pascal"},
		{5, "Volta"},
		{6, "Turing"},
		{7, "Ampere"},
		{8, "Ada"},
		{9, "Hopper"},
		{10, "Blackwell"},
		{11, "Orin"},
	};

	std::ostream& operator<<(std::ostream& s, APIType t)
	{
		if (t == API_TYPE_UNKNOWN)
		{
			s << "UNKNOWN";
			return s;
		}

		std::vector<String> apiNames;
		for (UI32 mask = 1; mask < API_TYPE_LAST; mask <<= 1)
		{
			switch (t & mask)
			{
#ifdef _WIN32
			case API_TYPE_DXGI:
				apiNames.push_back("DXGI");
				break;
#endif
#ifdef XPUINFO_USE_DXCORE
			case API_TYPE_DXCORE:
				apiNames.push_back("DXCore");
				break;
#endif
#ifdef _WIN32
			case API_TYPE_DX11_INTEL_PERF_COUNTER:
				apiNames.push_back("Intel Device Information");
				break;
#endif
#ifdef XPUINFO_USE_IGCL
			case API_TYPE_IGCL:
				apiNames.push_back("IGCL");
				break;
#endif
#ifdef XPUINFO_USE_LEVELZERO
			case API_TYPE_LEVELZERO:
				apiNames.push_back("Level Zero");
				break;
#endif
#ifdef XPUINFO_USE_OPENCL
			case API_TYPE_OPENCL:
				apiNames.push_back("OpenCL");
				break;
#endif
#ifdef XPUINFO_USE_SETUPAPI
			case API_TYPE_SETUPAPI:
				apiNames.push_back("SetupAPI");
				break;
#endif
#ifdef XPUINFO_USE_NVML
			case API_TYPE_NVML:
				apiNames.push_back("NVML");
				break;
#endif
#ifdef __APPLE__
            case API_TYPE_METAL:
                apiNames.push_back("Metal");
                break;
#endif
#ifdef XPUINFO_USE_WMI
			case API_TYPE_WMI:
				apiNames.push_back("WMI");
				break;
#endif
#ifdef XPUINFO_USE_IGCL
			case API_TYPE_IGCL_L0:
                apiNames.push_back("IGCL_L0");
                break;
#endif
			case API_TYPE_DESERIALIZED:
				apiNames.push_back("Deserialized");
				break;
			}
		}
		size_t i = 0;
		for (; i < apiNames.size(); ++i)
		{
			s << apiNames[i];
			if (i < apiNames.size() - 1)
			{
				s << ", ";
			}
		}
		return s;
	}

	std::ostream& operator<<(std::ostream& s, DeviceType t)
	{
		std::stringstream str;
		switch (t)
		{
		case DEVICE_TYPE_CPU:
			str << "CPU";
			break;
		case DEVICE_TYPE_GPU:
			str << "GPU";
			break;
		case DEVICE_TYPE_NPU:
			str << "NPU";
			break;
		case DEVICE_TYPE_OTHER:
			str << "Other";
			break;
		default:
			str << "Unknown";
		}
		s << str.str();
		return s;
	}

bool PCIAddressType::valid() const
{
	return isValidPCIAddr(*this);
}

bool PCIAddressType::GetFromWStr(const WString& inStr)
{
	// PCI bus 0, device 2, function 0
	std::wistringstream ins(inStr);
	domain = 0;
	WString tStr;
	ins >> tStr;
	ins >> tStr;
	ins >> bus;
	if (ins.good())
	{
		ins >> tStr; //,
		ins >> tStr; //device
		ins >> device;
		if (ins.good())
		{
			ins >> tStr;
			ins >> tStr;
			ins >> function;
			if (ins.eof() || ins.good())
			{
				return valid();
			}
		}
	}
	return false;
}

// From https://github.com/GameTechDev/gpudetect/blob/master/GPUDetect.cpp#L448
// Get driver version from LUID and registry
DeviceDriverVersion::DeviceDriverVersion(LUID 
#if defined(_WIN32) && !defined(_M_ARM64)
	inLuid
#endif
	) : mRawVersion(0ULL)
{
#if defined(_WIN32) && !defined(_M_ARM64)
	HKEY dxKeyHandle = nullptr;
	DWORD numOfAdapters = 0;

	if (!inLuid.LowPart && !inLuid.HighPart)
	{
		// Fail because registry may contain zero values.
		return; // Invalid
	}

	LSTATUS returnCode = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\DirectX"), 0, KEY_READ, &dxKeyHandle);

	if (returnCode != ERROR_SUCCESS)
	{
		return; // GPUDETECT_ERROR_REG_NO_D3D_KEY;
	}

	// Find all subkeys

	DWORD subKeyMaxLength = 0;

	returnCode = ::RegQueryInfoKey(
		dxKeyHandle,
		nullptr,
		nullptr,
		nullptr,
		&numOfAdapters,
		&subKeyMaxLength,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr
	);

	if (returnCode != ERROR_SUCCESS)
	{
		return; // GPUDETECT_ERROR_REG_GENERAL_FAILURE;
	}

	subKeyMaxLength += 1; // include the null character

	uint64_t driverVersionRaw = 0;

	std::vector<TCHAR> subKeyName(subKeyMaxLength, 0);

	for (DWORD i = 0; i < numOfAdapters; ++i)
	{
		DWORD subKeyLength = subKeyMaxLength;

		returnCode = ::RegEnumKeyEx(
			dxKeyHandle,
			i,
			&subKeyName[0],
			&subKeyLength,
			nullptr,
			nullptr,
			nullptr,
			nullptr
		);

		if (returnCode == ERROR_SUCCESS)
		{
			LUID adapterLUID = {};
			DWORD qwordSize = sizeof(uint64_t);

			returnCode = ::RegGetValue(
				dxKeyHandle,
				&subKeyName[0],
				TEXT("AdapterLuid"),
				RRF_RT_QWORD,
				nullptr,
				&adapterLUID,
				&qwordSize
			);

			if (returnCode == ERROR_SUCCESS // If we were able to retrieve the registry values
				&& adapterLUID.HighPart == inLuid.HighPart && adapterLUID.LowPart == inLuid.LowPart) // and if the vendor ID and device ID match
			{
				// We have our registry key! Let's get the driver version num now

				returnCode = ::RegGetValue(
					dxKeyHandle,
					&subKeyName[0],
					TEXT("DriverVersion"),
					RRF_RT_QWORD,
					nullptr,
					&driverVersionRaw,
					&qwordSize
				);

				if (returnCode == ERROR_SUCCESS)
				{
					mValid = true;
					mRawVersion = driverVersionRaw;
					break;
				}
			}
		}
	}

	returnCode = ::RegCloseKey(dxKeyHandle);
	XPUINFO_REQUIRE(returnCode == ERROR_SUCCESS);
#endif
}

static int countNumChar(const char c, const std::string& str)
{
	int numMatch = 0;
	size_t pos = 0;
	while (1)
	{
		size_t newPos = str.find(c, pos);
		if (newPos == str.npos)
		{
			break;
		}
		if (newPos > pos)
		{
			++numMatch;
			pos = newPos + 1;
		}
	}
	return numMatch;
}

DeviceDriverVersion DeviceDriverVersion::FromString(const std::string& version)
{
	// Handle x.y.z.w or just z.w
	int numDots = countNumChar('.', version);

	std::istringstream istr(version);
	std::vector<std::uint16_t> verWords;
	verWords.resize(numDots + 1);

	int w = 0;
	char sep;
	istr >> verWords[w++];
	for (; w < verWords.size(); ++w)
	{
		if (istr.bad())
		{
			break;
		}
		istr >> sep;
		if (sep == '.' && !istr.bad())
		{
			istr >> verWords[w];
		}
	}

	if (!istr.bad())
	{
		UI64 verRaw = 0;
		for (w = 0; w < verWords.size(); ++w)
		{
			verRaw = (verRaw << 16) | verWords[w];
		}
		return DeviceDriverVersion(verRaw);
	}
	return DeviceDriverVersion(LUID{});
}

const DeviceDriverVersion& DeviceDriverVersion::GetMax()
{
	static const auto verInfinite = XI::DeviceDriverVersion(XI::UI64(0xffffffff));
	return verInfinite;
}

const DeviceDriverVersion& DeviceDriverVersion::GetMin()
{
	static const auto verZero = XI::DeviceDriverVersion(XI::UI64(0));
	return verZero;
}

bool DeviceDriverVersion::InRange(const DeviceDriverVersion::VersionRange& range) const
{
	XPUINFO_REQUIRE(mValid);
	XPUINFO_REQUIRE(range.first.mValid);
	XPUINFO_REQUIRE(range.second.mValid);

	// this >= first && this <= second == this >= first && second >= this
	if (CompareGE(range.first) && range.second.CompareGE(*this))
	{
		return true;
	}

	return false;
}

String DeviceDriverVersion::GetAsString() const
{
	if (mValid)
	{
		std::stringstream outStr;
		outStr << (unsigned int)((mRawVersion & 0xFFFF000000000000) >> 16 * 3) << "." <<
			(unsigned int)((mRawVersion & 0x0000FFFF00000000) >> 16 * 2) << "." <<
			(unsigned int)((mRawVersion & 0x00000000FFFF0000) >> 16 * 1) << "." <<
			(unsigned int)((mRawVersion & 0x000000000000FFFF));
		return outStr.str();
	}
	else
	{
		return "InvalidVersion";
	}
}

WString DeviceDriverVersion::GetAsWString() const
{
	if (mValid)
	{
		std::wstringstream outStr;
		outStr << (unsigned int)((mRawVersion & 0xFFFF000000000000) >> 16 * 3) << "." <<
			(unsigned int)((mRawVersion & 0x0000FFFF00000000) >> 16 * 2) << "." <<
			(unsigned int)((mRawVersion & 0x00000000FFFF0000) >> 16 * 1) << "." <<
			(unsigned int)((mRawVersion & 0x000000000000FFFF));
		return outStr.str();
	}
	else
	{
		return L"InvalidVersion";
	}
}

bool DeviceDriverVersion::CompareGE(const DeviceDriverVersion& rhs) const
{
	UI64 inBuildNumberLast4Digits = (rhs.mRawVersion & 0x000000000000FFFF);
	bool last4ge = (std::uint16_t)((mRawVersion & 0x000000000000FFFF)) >= inBuildNumberLast4Digits;

	std::uint16_t curRelease = (std::uint16_t)((mRawVersion & 0x00000000FFFF0000) >> 16 * 1);
	std::uint16_t inReleaseField = (std::uint16_t)((rhs.mRawVersion & 0x00000000FFFF0000) >> 16 * 1);

	return (curRelease > inReleaseField) || ((curRelease >= inReleaseField) && last4ge);
}

bool DeviceDriverVersion::AtLeast(std::uint16_t inBuildNumberLast4Digits, std::uint16_t inReleaseField) const
{
	bool last4ge = (std::uint16_t)((mRawVersion & 0x000000000000FFFF)) >= inBuildNumberLast4Digits;
	std::uint16_t curRelease = (std::uint16_t)((mRawVersion & 0x00000000FFFF0000) >> 16 * 1);
	if (inReleaseField == kReleaseNumber_Ignore)
	{
		// Intel drivers have format a.b.c.xxxx where a, b are not used for versioning.  For build number, 
		// compare only the last 4 digits when c <= 100.  Builds with c > 100 (e.g. a.b.101.xxxx) should pass this check.
		return (curRelease > 100) || last4ge;
	}
	else
	{
		return (curRelease > inReleaseField) || ((curRelease >= inReleaseField) && last4ge);
	}
}

bool PCIAddressType::operator==(const PCIAddressType& inRHS) const
{
	return (domain == inRHS.domain) &&
		(bus == inRHS.bus) &&
		(device == inRHS.device) &&
		(function == inRHS.function);
}

bool RuntimeVersion::operator!=(const RuntimeVersion& l) const
{
	return (major != l.major) || (minor != l.minor) ||
		(build != l.build || (productVersion != l.productVersion));
}

bool RuntimeVersion::operator==(const RuntimeVersion& l) const
{
	return !operator!=(l);
}

#if defined(_WIN32) && !defined(_M_ARM64)
void XPUInfo::initDXGI(APIType initMask)
{
    DWORD dxgiFactoryFlags = 0;
#ifdef _DEBUG
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    WRL::ComPtr<IDXGIFactory4> currentFactory;
    CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(currentFactory.GetAddressOf())); // List of devices created here - need to re-init if devices change
    WRL::ComPtr<IDXGIAdapter1> adapter;

    for (UINT adapterIndex = 0;
        currentFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND;
        ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc{};
        HRESULT hres = adapter->GetDesc1(&desc);
        if (SUCCEEDED(hres))
        {
            if ((desc.VendorId == 0x1414) && (desc.DeviceId == 0x8c))
            {
                continue; // Skip "Microsoft Basic Render Driver"
            }
            else
            {
				{
					DebugStreamW dStr(false);
					dStr << L"Adapter " << adapterIndex << L": " << desc.Description << L", Vendor = " << std::hex << desc.VendorId << std::dec << std::endl;
				}
				//LARGE_INTEGER ver;
				//hres = adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &ver); // Essentially the DX10 driver version - seems to be valid even for RDP virtual adapter
				DevicePtr newDevice(new Device(adapterIndex, &desc));
				if (!!newDevice && newDevice->driverVersion().Valid())
				{
					UI64 uiLuid = newDevice->getLUID();
					auto newIt = m_Devices.insert(std::make_pair(uiLuid, newDevice));
					if (!(m_UsedAPIs & API_TYPE_DXGI))
						m_UsedAPIs = m_UsedAPIs | API_TYPE_DXGI;

					if ((initMask & API_TYPE_DX11_INTEL_PERF_COUNTER) && 
						newDevice->IsVendor(kVendorId_Intel)) // Early-out for non-Intel devices
					{
						newIt.first->second->initDXIntelPerfCounter(adapter.Get());

						if (!(m_UsedAPIs & API_TYPE_DX11_INTEL_PERF_COUNTER)
							&& (newIt.first->second->getCurrentAPIs() & API_TYPE_DX11_INTEL_PERF_COUNTER))
						{
							m_UsedAPIs = m_UsedAPIs | API_TYPE_DX11_INTEL_PERF_COUNTER;
						}
					}
				}
            }
        }
    }

	// Show displays connected
#if 0
	{
		std::ostream& dStr = std::cout;
		int maxDevNum = 0;
		DISPLAY_DEVICEA tempDD, monitor;
		ZeroMemory(&tempDD, sizeof(DISPLAY_DEVICEA));
		tempDD.cb = sizeof(DISPLAY_DEVICEA);
		monitor.cb = sizeof(DISPLAY_DEVICEA);
		DISPLAY_DEVICEA tempDD2;
		ZeroMemory(&tempDD2, sizeof(DISPLAY_DEVICEA));
		tempDD2.cb = sizeof(DISPLAY_DEVICEA);
		BOOL bRet = TRUE;
		while (bRet)
		{
			bRet = EnumDisplayDevicesA(NULL, maxDevNum, &tempDD, 0);
			if (tempDD.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
			{
				dStr << "Display " << maxDevNum << ": " << tempDD.DeviceName;
				if (tempDD.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
				{
					dStr << " *PRIMARY*";
				}
				dStr << "\n\t" << tempDD.DeviceString << " - " << tempDD.DeviceID << "\n\t   - " << tempDD.DeviceKey;
				// TODO: Get DeviceKey/DriverDate for date string - or figure out how to parse DriverDateData
				// ? https://learn.microsoft.com/en-us/windows-hardware/drivers/install/devpkey-device-driverdate
				bRet = EnumDisplayDevicesA(tempDD.DeviceName, 0, &monitor, 0);
				dStr << "\n\t     on " << monitor.DeviceString;

				DEVMODEA devMode;
				memset(&devMode, 0, sizeof(devMode));
				devMode.dmSize = sizeof(devMode);
				bRet = EnumDisplaySettingsExA(tempDD.DeviceName, ENUM_CURRENT_SETTINGS, &devMode, 0);
				if (bRet)
				{
					dStr << ", " << devMode.dmPelsWidth << "x" << devMode.dmPelsHeight << " " << devMode.dmBitsPerPel << "bpp @ " << devMode.dmDisplayFrequency << "Hz";
				}
				dStr << std::endl;
			}
			++maxDevNum;
		}
	}
#endif
}
#endif // WIN32

void XPUInfo::finalInitDXGI()
{
	// If no other APIs have determined UMA, guess from memory sizes
	const UI64 k256MB = 256 * 1024 * 1024ULL;
	const UI64 k2GB = 2 * 1024 * 1024 * 1024ULL;
	for (auto& it : m_Devices)
	{
		if (it.second->m_props.UMA == UMA_UNKNOWN)
		{
			if ((it.second->m_props.dxgiDesc.DedicatedVideoMemory <= k256MB) &&
				(it.second->m_props.dxgiDesc.SharedSystemMemory >= k2GB))
			{
				// For example, if DXCore's DXCoreAdapterProperty::IsIntegrated is not supported for NPU, this will mark it as integrated
				it.second->m_props.UMA = UMA_INTEGRATED;
			}
			else if (it.second->m_props.dxgiDesc.DedicatedVideoMemory >= k2GB)
			{
				it.second->m_props.UMA = NONUMA_DISCRETE;
			}
		}
	}
}

// Note: HybridDetect is one of the bigger contributors to binary size (at least on Win/x64) - consider a streamlined implementation for clients minimizing binary size
DeviceCPU::DeviceCPU() : DeviceBase(DeviceBase::kAdapterIndex_CPU, DEVICE_TYPE_CPU), 
	m_initialMXCSR(getcsr())
{
	m_pProcInfo.reset(new HybridDetect::PROCESSOR_INFO);
	if (m_pProcInfo)
	{
		HybridDetect::GetProcessorInfo(*m_pProcInfo);
	}
}

UI32 DeviceCPU::getcsr()
{
	static const int MXCSR_CONTROL_MASK = ~0x3f; /* all except last six status bits */

	UI32 mxcsr;
#if XPUINFO_CPU_X86_64
#ifdef _WIN32
	mxcsr = _mm_getcsr();
#else
	__asm__ __volatile__(
		"stmxcsr %0"
		: "=m"(mxcsr)
	);
#endif
#else
	mxcsr = 0;
#endif
	return mxcsr & MXCSR_CONTROL_MASK;
}

WString DeviceCPU::name() const
{
	if (m_pProcInfo)
	{
		return convert(m_pProcInfo->brandString);
	}
	return WString();
}

Device::Device(UI32 inIndex, DXGI_ADAPTER_DESC1* pDesc, DeviceType inType, APIType inAPI,
	XI::UI64 rawDriverVerion) : DeviceBase(inIndex)
{
	if (pDesc)
	{
		m_props.dxgiDesc = *pDesc; // copy
		m_type = inType;
		validAPIs = validAPIs | inAPI;

		if (!rawDriverVerion)
		{
			m_pDriverVersion.reset(new DeviceDriverVersion(m_props.dxgiDesc.AdapterLuid));
		}
		else
		{
			m_pDriverVersion.reset(new DeviceDriverVersion(rawDriverVerion));
		}
#if defined(_WIN32) && defined(_DEBUG)
		{
			DebugStreamW dStr(false);
			dStr << L"Device: " << name() << L", LUID = " << std::hex << getLUID() << std::dec << L", Version = " << m_pDriverVersion->GetAsWString() << std::endl;
		}
#endif
        
		m_props.DedicatedMemorySize = m_props.dxgiDesc.DedicatedVideoMemory;
		m_props.SharedMemorySize = m_props.dxgiDesc.SharedSystemMemory;
	}
}
Device::~Device()
{
}

static const DeviceDriverVersion S_NullDriverVersion(LUID{});

const DeviceDriverVersion& Device::driverVersion() const
{
	if (m_pDriverVersion)
	{
		return *m_pDriverVersion;
	}
	else
	{
		return S_NullDriverVersion;
	}
}

DeviceProperties::DeviceProperties()
{
	// Initialize to -1 to indicate unknown across all members of union
	memset(&VendorSpecific, -1, sizeof(VendorSpecific));
};

const char* DeviceProperties::getDeviceGenerationName() const
{
	if ((DeviceGenerationAPI == API_TYPE_DX11_INTEL_PERF_COUNTER) || (DeviceGenerationAPI == API_TYPE_SETUPAPI))
	{
		for (int i = S_numGenNames-1; i >= 0; --i)
		{
			if (S_GenNameMap[i].gen == (UI32)DeviceGenerationID)
			{
				return S_GenNameMap[i].name;
			}
		}
	}
	else if ((DeviceGenerationAPI == API_TYPE_OPENCL) || (DeviceGenerationAPI == API_TYPE_LEVELZERO))
	{
		for (int i = S_numGenNames - 1; i >= 0; --i)
		{
			if (S_GenNameMap[i].ipvu.ipVersion == (UI32)DeviceGenerationID)
			{
				return S_GenNameMap[i].name;
			}
		}
	}
	else if (DeviceGenerationAPI == API_TYPE_NVML)
	{
		std::unordered_map<XI::UI32, XI::String>::const_iterator it = S_nVArchNames.find(DeviceGenerationID);
		if (it != S_nVArchNames.end())
		{
			return it->second.c_str();
		}
	}
	return nullptr;
}

UI64 DeviceProperties::getVideoMemorySize() const
{
	return (UMA == UMA_INTEGRATED) ? dxgiDesc.DedicatedVideoMemory + dxgiDesc.SharedSystemMemory :
		dxgiDesc.DedicatedVideoMemory;
}

#define XPUINFO_BUILD_TIMESTAMP_INTERNAL __DATE__ " " __TIME__

XPUInfo::XPUInfo(APIType initMask, const RuntimeNames& runtimeNamesToTrack, size_t clientClassSize, const char* buildTimestamp) : 
	m_InitAPIs(initMask), m_UsedAPIs(API_TYPE_UNKNOWN), m_clientBuildTimestamp(buildTimestamp), m_internalBuildTimestamp(XPUINFO_BUILD_TIMESTAMP_INTERNAL)
{
	// Verify class size matches between internal lib and clients
	const size_t libClassSize = sizeof(XPUInfo);
	XPUINFO_REQUIRE(libClassSize == clientClassSize);
	if (!(initMask & API_TYPE_DESERIALIZED))
	{
		// Skip if this will be deserialized
		m_pCPU.reset(new DeviceCPU);
	}

#if defined(_WIN32) && defined(XPUINFO_USE_WMI)
	std::unique_ptr<std::thread> wmiThreadPtr;
	if (initMask & API_TYPE_WMI)
	{
		wmiThreadPtr.reset(new std::thread([&]() { initWMI(); }));
	}
#endif

#if defined(_WIN32) && !defined(_M_ARM64)
	if (initMask & (API_TYPE_DXGI | API_TYPE_DX11_INTEL_PERF_COUNTER))
	{
		initDXGI(initMask); // Must be first
	}
#endif

#ifdef XPUINFO_USE_DXCORE
	if ((initMask & API_TYPE_DXCORE) && hasDXCore())
	{
		initDXCore();
	}
#endif

#ifdef XPUINFO_USE_IGCL
	if (initMask & API_TYPE_IGCL)
	{
		initIGCL((initMask & API_TYPE_IGCL_L0) != 0);
	}
#endif
    
#ifdef XPUINFO_USE_OPENCL
	if (initMask & API_TYPE_OPENCL)
	{
		// Only try OpenCL if a GPU has already been detected since OpenCL.dll otherwise might not exist
		for (const auto& [luid, dev] : m_Devices)
		{
			if (dev->getType() == DeviceType::DEVICE_TYPE_GPU)
			{
				initOpenCL();
				break;
			}
		}
	}
#endif

#ifdef XPUINFO_USE_LEVELZERO
	if (initMask & API_TYPE_LEVELZERO)
	{
		// Only run if at least 1 Intel GPU found - delay-loading ze_loader.dll
		for (const auto& [luid, dev] : m_Devices)
		{
			if (dev->IsVendor(kVendorId_Intel))
			{
				initL0();
				break;
			}
		}
	}
#endif

#ifdef XPUINFO_USE_SETUPAPI
	if (initMask & API_TYPE_SETUPAPI)
	{
		m_pSetupInfo.reset(new SetupDeviceInfo);
		bool bSDIMatchFound = false;
		for (auto& devPair : m_Devices)
		{
			auto& device = devPair.second;
			DriverInfoPtr pSDI = m_pSetupInfo->getByLUID(device->getLUID());
			if (!pSDI)
			{
				if (device->m_props.PCIAddress.valid())
				{
					pSDI = m_pSetupInfo->getAtAddress(device->m_props.PCIAddress);
				}
				else
				{
					// Match name
					// TODO: What should happen if multiple devices have the same name?  (i.e. 2x RTX 3080?)
					// For now, assume they have the same driver and other properties.
					pSDI = m_pSetupInfo->getByName(device->name());
				}
			}
			if (pSDI)
			{
				if (!device->m_props.pDriverInfo)
				{
					device->m_props.pDriverInfo = pSDI;
					device->validAPIs = device->validAPIs | API_TYPE_SETUPAPI;
					bSDIMatchFound = true;
					if (device->m_pDriverVersion && !device->m_pDriverVersion->Valid())
					{
						// LUID lookup in registry failed
						// Set DriverVersion from SetupAPI version string
						*device->m_pDriverVersion = DeviceDriverVersion::FromString(convert(pSDI->DriverVersion));
					}
				}
				if (!device->m_props.PCIAddress.valid())
				{
					device->m_props.PCIAddress = pSDI->LocationInfo;
				}
				if (device->IsVendor(kVendorId_Intel))
				{
					if (device->getProperties().DeviceGenerationAPI == API_TYPE_UNKNOWN)
					{
						String infName(convert(pSDI->DriverInfSection));

						for (int i = S_numGenNames - 1; i >= 0; --i)
						{
							if (S_GenNameMap[i].infName && (infName.find(S_GenNameMap[i].infName) == 0))
							{
								device->m_props.DeviceGenerationID = S_GenNameMap[i].gen;
								device->m_props.DeviceGenerationAPI = API_TYPE_SETUPAPI;
								break;
							}
						}

					}
				}
			}
		}
		if (bSDIMatchFound)
		{
			m_UsedAPIs = m_UsedAPIs | API_TYPE_SETUPAPI;
		}
	}
#endif // XPUINFO_USE_SETUPAPI

	// NVML is after SetupAPI to match PCIAddressType
#ifdef XPUINFO_USE_NVML
	if (initMask & API_TYPE_NVML)
	{
#ifdef __linux__
		initNVML();
#else
		// Only run if at least 1 nVidia GPU found - delay-loading nvml.dll
		for (const auto& [luid, dev] : m_Devices)
		{
			if (dev->IsVendor(kVendorId_nVidia))
			{
				initNVML();
				break;
			}
		}
#endif // linux
	}
#endif

#ifdef __APPLE__
    if (initMask & API_TYPE_METAL)
    {
        initMetal();
    }
#endif
    
	// D3D12 CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1) has UMA flags
    // L0 has integrated flag, num EUs, clockRate in ze_device_properties_t
    // TODO: Use IGCL ctl_power_properties_t or ctlPowerGetLimits() for TDP info if not using API_TYPE_DX11_INTEL_PERF_COUNTER

	if (initMask & API_TYPE_DXGI)
	{
		finalInitDXGI();
	}

#ifdef XPUINFO_USE_RUNTIMEVERSIONINFO
	if (!(initMask & API_TYPE_DESERIALIZED))
	{
		getRuntimeVersions(runtimeNamesToTrack);
	}
#endif

#if defined(_WIN32) && defined(XPUINFO_USE_WMI)
	if (wmiThreadPtr.get())
	{
		wmiThreadPtr->join();
	}
#endif
#ifdef XPUINFO_USE_SYSTEMEMORYINFO
	// Init after SystemMemoryInfo (WMI on Win)
	m_pMemoryInfo.reset(new SystemMemoryInfo);
#endif // XPUINFO_USE_SYSTEMEMORYINFO
}

#if defined(XPUINFO_USE_RUNTIMEVERSIONINFO)
String RuntimeVersion::getAsString() const
{
	return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
}

void XPUInfo::getRuntimeVersions(const RuntimeNames& runtimeNames)
{
#ifdef _WIN32
	RuntimeVersion ver;
	for (const auto& file : runtimeNames)
	{
		if (Win::GetVersionFromFile(file, ver))
		{
			// Assume this map starts off empty
			m_RuntimeVersions.insert(std::make_pair(String(file), ver));
		}
	}
#endif
}
#endif // XPUINFO_USE_RUNTIMEVERSIONINFO

XPUInfo::~XPUInfo()
{
}

template <APIType APITYPE>
bool XPUInfo::getDevice(UI64 inLUID, typename API_Traits<APITYPE>::API_handle_type* outTypePtr)
{
	return false;
}

template <>
bool XPUInfo::getDevice<API_TYPE_LEVELZERO>(UI64 inLUID, typename API_Traits<API_TYPE_LEVELZERO>::API_handle_type* outTypePtr)
{
	if (outTypePtr)
	{
		auto it = m_Devices.find(inLUID);
		if (it != m_Devices.end())
		{
			if (it->second->m_L0Device)
			{
				*outTypePtr = it->second->m_L0Device;
				return true;
			}
		}
	}
	return false;
}

template <>
bool XPUInfo::getDevice<API_TYPE_IGCL>(UI64 inLUID, typename API_Traits<API_TYPE_IGCL>::API_handle_type* outTypePtr)
{
	if (outTypePtr)
	{
		auto it = m_Devices.find(inLUID);
		if (it != m_Devices.end())
		{
			if (it->second->m_hIGCLAdapter)
			{
				*outTypePtr = it->second->m_hIGCLAdapter;
				return true;
			}
		}
	}
	return false;
}

DevicePtr XPUInfo::getDeviceInternal(UI64 inLUID)
{
	auto it = m_Devices.find(inLUID);
	if (it != m_Devices.end())
	{
		return it->second;
	}
	return DevicePtr();
}

DevicePtr XPUInfo::getDeviceInternal(const char* inNameSubString)
{
	WString lowerMatchStr = convert(toLower(inNameSubString));
	auto it = m_Devices.begin();
	for (; it != m_Devices.end(); ++it)
	{
		WString devName(toLower(WString(it->second->name())));
		if (devName.find(lowerMatchStr) != WString::npos)
		{
			return it->second;
		}
	}
	return DevicePtr();
}

const DevicePtr XPUInfo::getDevice(UI64 inLUID) const
{
	auto it = m_Devices.find(inLUID);
	if (it != m_Devices.end())
	{
		return it->second;
	}
	return DevicePtr();
}

const DevicePtr XPUInfo::getDevice(const char* inNameSubString) const
{
	WString lowerMatchStr = convert(toLower(inNameSubString));
	auto it = m_Devices.begin();
	for (; it != m_Devices.end(); ++it)
	{
		WString devName(toLower(WString(it->second->name())));
		if (devName.find(lowerMatchStr) != WString::npos)
		{
			return it->second;
		}
	}
	return DevicePtr();
}

const DevicePtr XPUInfo::getDeviceByIndex(UI32 inIndex) const
{
	for (const auto& [luidUnused, dev] : m_Devices)
	{
		if (dev->getAdapterIndex() == inIndex)
		{
			return dev;
		}
	}
	return DevicePtr();
}

#if defined(_WIN32) && !defined(_M_ARM64)
float DriverInfo::SystemTimeToYears(const SYSTEMTIME& inSysTime) 
{
    float years = inSysTime.wYear + inSysTime.wMonth / 12.0f + inSysTime.wDay / 365.25f;
    return years;
}

// Return age and driver date converted to SYSTEMTIME 
float DriverInfo::DriverAgeInYears(const FILETIME& inFileTime, SYSTEMTIME& outSysTime)
{
	SYSTEMTIME curSysTime;
	GetSystemTime(&curSysTime);
	if (FileTimeToSystemTime(&inFileTime, &outSysTime))
	{
		float yearsCur = SystemTimeToYears(curSysTime);
		float yearsDriver = SystemTimeToYears(outSysTime);
		return yearsCur - yearsDriver;
	}
	return -0.0f; // Use std::signbit to detect error
}

float DriverInfo::DriverAgeInYears() const
{
	SYSTEMTIME sysTime;
	return DriverAgeInYears(DriverDate, sysTime);
}
#elif __APPLE__ || __linux__
float DriverInfo::DriverAgeInYears() const
{
    return 0.f; // TODO
}
#endif

std::ostream& operator<<(std::ostream& ostr, const DevicePtr& xiDev)
{
	if (!!xiDev)
	{
		ostr << *xiDev;
	}
	return ostr;
}

std::ostream& operator<<(std::ostream& ostr, const Device& xiDev)
{
	auto& devProps = xiDev.getProperties();
    const char* IndexName =
#ifdef _WIN32
        "DXGI Index";
#else
        "Index";
#endif
    
	ostr << "XPUInfo[" << IndexName << "=" << xiDev.getAdapterIndex() << ", LUID=0x" << std::hex << xiDev.getLUID() << std::dec << "]:\n";
	ostr << "\tName: " << XI::convert(xiDev.name()) << std::endl;
    if (xiDev.driverVersion().Valid())
    {
        ostr << "\tDriver Version: " << xiDev.driverVersion().GetAsString() << std::endl;
    }
	if (devProps.pDriverInfo)
	{
#if defined(_WIN32) && !defined(_M_ARM64)
		SYSTEMTIME sysTime;
		float driverAge = devProps.pDriverInfo->DriverAgeInYears(devProps.pDriverInfo->DriverDate, sysTime);
		if (!std::signbit(driverAge)) // not negative
		{
			auto& dStr = ostr;
			dStr << "\tDriver Date: ";
			dStr << sysTime.wMonth << "/" << sysTime.wDay << "/" << sysTime.wYear;
			auto oldPrec = dStr.precision(2);
			dStr << " (" << driverAge << " years old)\n";
			dStr.precision(oldPrec);
		}
		driverAge = devProps.pDriverInfo->DriverAgeInYears(devProps.pDriverInfo->InstallDate, sysTime);
		if (!std::signbit(driverAge)) // not negative
		{
			auto& dStr = ostr;
			dStr << "\tInstall Date: ";
			dStr << sysTime.wMonth << "/" << sysTime.wDay << "/" << sysTime.wYear;
			auto oldPrec = dStr.precision(2);
			dStr << " (" << driverAge << " years)\n";
			dStr.precision(oldPrec);
		}
#endif
	}
	ostr << "\tType: " << xiDev.getType();
	if (devProps.UMA != XI::UMA_UNKNOWN)
	{
		ostr << ", " << ((devProps.UMA == XI::UMA_INTEGRATED) ? "Integrated" : "Discrete");
	}
	if (devProps.IsHighPerformance > 0)
	{
		ostr << ", HighPerformance";
	}
	if (devProps.IsMinimumPower > 0)
	{
		ostr << ", MinimumPower";
	}
	if (devProps.IsDetachable > 0)
	{
		ostr << ", Detachable";
	}
	ostr << std::endl;
	if ((devProps.DedicatedMemorySize != XI::UI64(-1LL)) /*&& (devProps.DedicatedMemorySize > 0ULL)*/)
	{
		ostr << "\tMemory (MB): Dedicated = " << devProps.DedicatedMemorySize / (1024ULL * 1024);
		if (devProps.SharedMemorySize != XI::UI64(-1LL))
		{
			ostr << ", Shared = " << devProps.SharedMemorySize / (1024ULL * 1024);
		}
		ostr << std::endl;
	}
	if (devProps.MemoryBandWidthMax != -1)
	{
		ostr << "\tMax Memory Bandwidth (GB/s): " << XI::I64(devProps.MemoryBandWidthMax / double(1024 * 1024 * 1024)) << std::endl;
	}
	ostr << "\tAPIs: " << xiDev.getCurrentAPIs() << std::endl;
	if (devProps.PCIReBAR.valid)
	{
		ostr << "\tResizable Bar: supported = " << devProps.PCIReBAR.supported << ", enabled = " << devProps.PCIReBAR.enabled << std::endl;
	}
	if (devProps.PCIDeviceGen != -1)
	{
		ostr << "\tDEVICE:  PCI Gen " << devProps.PCIDeviceGen;
		if (devProps.PCIDeviceWidth != -1)
			ostr << ", Width " << devProps.PCIDeviceWidth;
		if (devProps.PCIDeviceMaxBandwidth != -1)
		{
			auto prec = ostr.precision();
			ostr.precision(4);
			ostr << ", Max Bandwidth = " << devProps.PCIDeviceMaxBandwidth / double(1024 * 1024 * 1024) << " GB/s";
			ostr.precision(prec);
		}
		ostr << std::endl;
	}

	if (devProps.PCICurrentGen != -1)
	{
		ostr << "\tCURRENT: PCI Gen " << devProps.PCICurrentGen;
		if (devProps.PCICurrentWidth != -1)
			ostr << ", Width " << devProps.PCICurrentWidth;
		if (devProps.PCICurrentMaxBandwidth != -1)
		{
			auto prec = ostr.precision();
			ostr.precision(4);
			ostr << ", Max Bandwidth = " << devProps.PCICurrentMaxBandwidth / double(1024 * 1024 * 1024) << " GB/s";
			ostr.precision(prec);
		}
		ostr << std::endl;
	}
	if (devProps.pDriverInfo)
	{
		if (devProps.pDriverInfo->DriverInfSection.length())
		{
			ostr << "\tDriver Inf Section: " << XI::convert(devProps.pDriverInfo->DriverInfSection) << std::endl;
		}
		if (devProps.pDriverInfo->DeviceInstanceId.length())
		{
			ostr << "\tDevice Instance ID: " << XI::convert(devProps.pDriverInfo->DeviceInstanceId) << std::endl;
		}
	}

	if (devProps.PCIAddress.valid())
	{
		ostr << "\tPCI Domain:Bus:Device:Function: " << devProps.PCIAddress.domain << ":" << devProps.PCIAddress.bus << ":" << devProps.PCIAddress.device << ":" << devProps.PCIAddress.function << std::endl;
	}

	if (devProps.FreqMaxMHz != -1)
	{
		ostr << "\tFrequency(MHz) Max = " << devProps.FreqMaxMHz;
		if (devProps.FreqMinMHz != -1)
		{
			ostr << ", Min = " << devProps.FreqMinMHz;
		}
		ostr << std::endl;
	}
	if (devProps.MediaFreqMaxMHz != -1)
	{
		ostr << "\tMedia Frequency(MHz) Max = " << devProps.MediaFreqMaxMHz;
		if (devProps.MediaFreqMinMHz != -1)
		{
			ostr << ", Min = " << devProps.MediaFreqMinMHz;
		}
		ostr << std::endl;
	}
	if (devProps.MemoryFreqMaxMHz != -1)
	{
		ostr << "\tMemory Frequency(MHz) Max = " << devProps.MemoryFreqMaxMHz;
		if (devProps.MemoryFreqMinMHz != -1)
		{
			ostr << ", Min = " << devProps.MemoryFreqMinMHz;
		}
		ostr << std::endl;
	}
	if (devProps.DeviceGenerationAPI != API_TYPE_UNKNOWN)
	{
		ostr << "\tGenerationAPI = " << devProps.DeviceGenerationAPI << std::endl;
	}
	if (devProps.DeviceGenerationID != -1)
	{
		ostr << "\tGeneration = ";
		const char* genName = devProps.getDeviceGenerationName();
		if (devProps.DeviceGenerationID >= 0)
		{
			bool bUseHex = (devProps.DeviceGenerationAPI != API_TYPE_DX11_INTEL_PERF_COUNTER);
			if (bUseHex)
				ostr << std::hex << "0x";
			ostr << devProps.DeviceGenerationID;
			if (bUseHex)
				ostr << std::dec;
			if (genName)
			{
				ostr << ", ";
			}
		}
		else if (devProps.DeviceGenerationID < 0)
		{
			// Find DeviceGenerationID in Map and use ipVersion instead
			for (int i = S_numGenNames - 1; i >= 0; --i)
			{
				if (S_GenNameMap[i].gen == (UI32)devProps.DeviceGenerationID)
				{
					if (S_GenNameMap[i].ipvu.ipVersion)
					{
						ostr << std::hex << "0x" << S_GenNameMap[i].ipvu.ipVersion << std::dec;
						if (genName)
						{
							ostr << ", ";
						}
					}
					break;
				}
			}
		}
		if (genName)
		{
			ostr << genName;
		}
		ostr << std::endl;
	}
	if (devProps.DeviceIPVersion != 0)
	{
		{
			SaveRestoreIOSFlags sr(ostr);
			ostr << "\tIP Version: 0x" << std::hex << std::setw(8) << std::right << std::setfill('0') << devProps.DeviceIPVersion;
		}
#if XPUINFO_HAS_CPP17
		auto IntelFamilyName = xiDev.getIntelGfxFamilyName();
		if (IntelFamilyName.has_value())
		{
			ostr << ", " << IntelFamilyName->second;
		}
#endif
		ostr << std::endl;
	}
	if (xiDev.IsVendor(kVendorId_Intel) && (devProps.VendorFlags.IntelFeatureFlags.FLAG_DP4A | devProps.VendorFlags.IntelFeatureFlags.FLAG_DPAS))
	{
		ostr << "\tFeature Flags: ";
		if (devProps.VendorFlags.IntelFeatureFlags.FLAG_DP4A)
			ostr << "DP4A ";
		if (devProps.VendorFlags.IntelFeatureFlags.FLAG_DPAS)
			ostr << "DPAS ";
		ostr << std::endl;
	}
	if (xiDev.IsVendor(kVendorId_nVidia))
	{
		auto ccc = devProps.VendorSpecific.nVidia.getCudaComputeCapability();
		if (ccc > 0)
		{
			ostr << "\tCUDA Compute Capability: " << ccc << std::endl;
		}
	}
	if (devProps.NumComputeUnits != -1)
	{
		ostr << "\tCompute Units: " << devProps.NumComputeUnits;
		if (devProps.ComputeUnitSIMDWidth != -1)
		{
			ostr << ", SIMD Width: " << devProps.ComputeUnitSIMDWidth;
		}
		ostr << std::endl;
	}
	if (devProps.PackageTDP != -1)
	{
		ostr << "\tPackage TDP (W): " << devProps.PackageTDP << std::endl;
	}
	return ostr;
}

inline const HybridDetect::LOGICAL_PROCESSOR_INFO* getLPIBySet(const HybridDetect::PROCESSOR_INFO* pi, 
	HybridDetect::CoreTypes setType, int idx = 0)
{
	if (pi)
	{
		auto coreIdIt = pi->cpuSets.find(setType);
		if (coreIdIt != pi->cpuSets.end())
		{
			auto coreInfoIt = std::find_if(pi->cores.begin(), pi->cores.end(),
				[&coreIdIt,idx](const HybridDetect::LOGICAL_PROCESSOR_INFO& lpi)
				{
					return (coreIdIt->second.size() > idx) && (lpi.id == coreIdIt->second[idx]);
				});
			if (coreInfoIt != pi->cores.end())
			{
				return &coreInfoIt[0];
			}
		}
	}
	return nullptr;
}

void DeviceCPU::printInfo(std::ostream& ostr, const SystemInfo* pSysInfo) const
{
	if (m_pProcInfo)
	{
		SaveRestoreIOSFlags srFlags(ostr);

		// CPU Info
		ostr << "CPU: " << m_pProcInfo->brandString << std::endl;
		ostr << "\tCores: " << m_pProcInfo->numPhysicalCores;
		if (m_pProcInfo->hybrid)
		{
			ostr << " (Hybrid)";
		}
		ostr << std::endl;
		ostr << "\tLogical: " << m_pProcInfo->numLogicalCores << std::endl;
#if defined(_WIN32) && defined(XPUINFO_USE_WMI)
		if (pSysInfo)
		{
			UI32 numEnabledCores = 0;
			UI32 numCores = 0;
			UI32 numLP = 0;
			for (const auto& p : pSysInfo->Processors)
			{
				numEnabledCores += p.NumberOfEnabledCores;
				numCores += p.NumberOfCores;
				numLP += p.NumberOfLogicalProcessors;
			}
			if (numEnabledCores && (numEnabledCores != m_pProcInfo->numPhysicalCores))
			{
				ostr << "\tCores Enabled: " << numEnabledCores << std::endl;
			}
			if (numCores && (numCores != m_pProcInfo->numPhysicalCores))
			{
				ostr << "\tSystem Cores: " << numCores << std::endl;
			}
			if (numLP && (numLP != m_pProcInfo->numLogicalCores))
			{
				ostr << "\tSystem Logical: " << numLP << std::endl;
			}
		}
#endif
		if (m_pProcInfo->cpuSets.size())
		{
			if ((m_pProcInfo->hybrid) && (m_pProcInfo->IsIntel()))
			{
				char fill = ostr.fill('0');
#if HYBRIDDETECT_CPU_X86_64
				const HybridDetect::LOGICAL_PROCESSOR_INFO* lpiCore = getLPIBySet(m_pProcInfo.get(), HybridDetect::CoreTypes::INTEL_CORE);
				const HybridDetect::LOGICAL_PROCESSOR_INFO* lpiAtom = getLPIBySet(m_pProcInfo.get(), HybridDetect::CoreTypes::INTEL_ATOM);
				ostr << "\t\tIntel(R) Core(TM): " << m_pProcInfo->GetCoreTypeCount(HybridDetect::CoreTypes::INTEL_CORE);
				ostr << "\t(0x" << std::hex << std::right << std::setw(16) << m_pProcInfo->coreMasks[HybridDetect::CoreTypes::INTEL_CORE] << ")" << std::dec;
				if (lpiCore && lpiCore->maximumFrequency)
				{
					ostr << ", " << lpiCore->baseFrequency << " / " << lpiCore->maximumFrequency << " (Base/Max MHz)";
				}
				ostr << std::endl;
				ostr << "\t\tIntel(R) Atom(TM): " << m_pProcInfo->GetCoreTypeCount(HybridDetect::CoreTypes::INTEL_ATOM);
				ostr << "\t(0x" << std::hex << std::right << std::setw(16) << m_pProcInfo->coreMasks[HybridDetect::CoreTypes::INTEL_ATOM] << ")" << std::dec;
				if (lpiAtom && lpiAtom->maximumFrequency)
				{
					ostr << ", " << lpiAtom->baseFrequency << " / " << lpiAtom->maximumFrequency << " (Base/Max MHz)";
				}
				ostr << std::endl;
#endif
				ostr.fill(fill); // reset
			}
			else
			{
				auto lpiCore = getLPIBySet(m_pProcInfo.get(), HybridDetect::CoreTypes::ANY);
				if (lpiCore && lpiCore->maximumFrequency)
				{
					ostr << "\tBase / Max Frequency (MHz): " << lpiCore->baseFrequency << " / " << lpiCore->maximumFrequency << std::endl;
				}
			}
		}

#if HYBRIDDETECT_CPU_X86_64
		// AVX512, AVX2, F16C, AVX, AES, SSE4.1
		ostr << "\tFeatures: ";
		if (m_pProcInfo->flags.AVX512_SKX_Supported())
		{
			ostr << "(AVX512_SKX) ";
		}
		else
		{
			if (m_pProcInfo->flags.AVX512F)
				ostr << "AVX512F ";
			if (m_pProcInfo->flags.AVX512VL)
				ostr << "AVX512VL ";
			if (m_pProcInfo->flags.AVX512CD)
				ostr << "AVX512CD ";
			if (m_pProcInfo->flags.AVX512DQ)
				ostr << "AVX512DQ ";
			if (m_pProcInfo->flags.AVX512BW)
				ostr << "AVX512BW ";
		}
		if (m_pProcInfo->flags.AVX512_State_Supported())
		{
			// Features not is AVX512_SKX subset
			if (m_pProcInfo->flags.AVX512_IFMA)
				ostr << "AVX512_IFMA ";
		}
		if (m_pProcInfo->flags.AVX2_Supported())
			ostr << "AVX2 ";
		if (m_pProcInfo->flags.F16C_Supported())
			ostr << "F16C ";
		if (m_pProcInfo->flags.AVX_Supported())
			ostr << "AVX ";
		if (m_pProcInfo->flags.SSE4_2)
			ostr << "SSE4.2 ";
		if (m_pProcInfo->flags.SSE4_1)
			ostr << "SSE4.1 ";
		if (m_pProcInfo->flags.SSSE3)
			ostr << "SSSE3 ";
		if (m_pProcInfo->flags.SSE3)
			ostr << "SSE3 ";
		ostr << std::endl;
#endif

		if (m_pProcInfo->numL3Caches)
		{
			for (unsigned int i = 0; i < m_pProcInfo->numL3Caches;)
			{
				for (const auto& c : m_pProcInfo->caches)
				{
					if (c.level == 3)
					{
						ostr << "\tLLC Size ";
						if (m_pProcInfo->numL3Caches > 1) // Multi-socket likely, number each
						{
							ostr << i << " ";
						}
						ostr << "= " << c.size / (1024 * 1024) << "MB, " << c.associativity << "-way associative, " << c.lineSize << "-byte lines\n";
						++i;
					}
				}
			}
		}
		else if (m_pProcInfo->numL2Caches)
		{
			for (unsigned int i = 0; i < m_pProcInfo->numL2Caches;)
			{
				for (const auto& c : m_pProcInfo->caches)
				{
					if (c.level == 2)
					{
						ostr << "\tL2 Size ";
						if (m_pProcInfo->numL2Caches > 1) // Multi-socket likely, number each
						{
							ostr << i << " ";
						}
						ostr << "= " << c.size / (1024) << "KB";
#if TARGET_CPU_X86_64
						ostr << ", " << c.associativity << "-way associative, " << c.lineSize << "-byte lines";
#endif
						ostr << std::endl;
						++i;
					}
				}
			}
		}

		const int basicCPUID = m_pProcInfo->cpuid_1_eax;
		if (basicCPUID && strlen(m_pProcInfo->vendorID))
		{
			ostr << "\t" << m_pProcInfo->vendorID << ": ";
			if (m_pProcInfo->IsIntel())
			{
				const int family = (basicCPUID >> 8) & 0xf;
				const int extModel = ((basicCPUID & 0xf0000) >> 12) | ((basicCPUID & 0xf0) >> 4);
				const int stepping = basicCPUID & 0xf;

				ostr << "Family = " << family;
				if (family == 6)
				{
					ostr << ", ExtModel = 0x" << std::hex << std::setw(2) << std::right << std::setfill('0') << extModel << std::dec;
				}
				ostr << ", Stepping = " << stepping << ", ";
			}
#if defined(__APPLE__) && !TARGET_CPU_X86_64
			ostr << std::endl;
#else
			ostr << "cpuid.1.eax = 0x" << std::hex << std::setw(8) << std::right << std::setfill('0') << basicCPUID << std::dec << std::endl;
#endif
			ostr << std::setfill(' ');
		}
	} // if (m_pProcInfo)
}

void XPUInfo::printSystemMemoryInfo(std::ostream& ostr) const
{
#ifdef XPUINFO_USE_SYSTEMEMORYINFO
	if (m_pMemoryInfo)
	{
		ostr << "System Memory:\n";
		const int leftCol = 41;
		ostr << std::left << std::setw(leftCol) << "\tInstalled Physical Memory (GB): " << std::setprecision(5) << BtoGB(m_pMemoryInfo->getInstalledPhysicalMemory()) << std::endl;
		ostr << std::left << std::setw(leftCol) << "\tTotal Physical Memory (GB): " << std::setprecision(5) << BtoGB(m_pMemoryInfo->getTotalPhysicalMemory()) << std::endl;
		ostr << std::left << std::setw(leftCol) << "\tAvailable Physical Memory At Init (GB): " << std::setprecision(5) << BtoGB(m_pMemoryInfo->getAvailablePhysicalMemoryAtInit()) << std::endl;
		ostr << std::left << std::setw(leftCol) << "\tPage Size (KB): " << std::setprecision(3) << BtoKB(m_pMemoryInfo->getPageSize()) << std::endl;
	}
#endif // XPUINFO_USE_SYSTEMEMORYINFO
}

void XPUInfo::printCPUInfo(std::ostream& ostr) const
{
	XPUINFO_REQUIRE(!!m_pCPU);
	m_pCPU->printInfo(ostr, m_pSystemInfo.get());
}

void XPUInfo::printSystemInfo(std::ostream& ostr) const
{
#if (defined(_WIN32) && defined(XPUINFO_USE_WMI)) || defined(__APPLE__)
	if (m_pSystemInfo)
	{
		ostr << "System Information:" << std::endl;
		ostr << *m_pSystemInfo << std::endl;
	}
#endif
}

void XPUInfo::printInfo(std::ostream& ostr) const
{
	ostr << "XPUInfo detected " << deviceCount() << " devices\n";

	DeviceMap::const_iterator it = getDeviceMap().begin();
	for (int i=0; i < getDeviceMap().size(); ++i,++it)
	{
		ostr << it->second;
		if (i < getDeviceMap().size() - 1)
		{
			ostr << std::endl;
		}
	}
	ostr << std::endl;
	printCPUInfo(ostr);
	ostr << std::endl;

	printSystemMemoryInfo(ostr);

	printSystemInfo(ostr);

#ifdef XPUINFO_USE_RUNTIMEVERSIONINFO
	if (m_RuntimeVersions.size())
	{
		ostr << "Runtime Version Info:\n";
		for (const auto& vi : getRuntimeVersionInfo())
		{
			// Preformat cols into strings
			std::ostringstream nameStr;

			nameStr << "\t" << vi.first << ":";

			// Output cols
			SaveRestoreIOSFlags srFlags(ostr);
			ostr << std::left << std::setw(28+8) << nameStr.str();
			ostr << std::left << std::setw(14) << vi.second.getAsString();
			if (vi.second.productVersion.size())
			{
				ostr << " (" << vi.second.productVersion << ")";
			}
			ostr << std::endl;
		}
	}
#endif // XPUINFO_USE_RUNTIMEVERSIONINFO

	{
		SaveRestoreIOSFlags srFlags(ostr);
		ostr << std::endl;
		ostr << std::left << std::setw(24) << "APIs requested at init:" << m_InitAPIs << std::endl;
		ostr << std::left << std::setw(24) << "APIs initialized: " << m_UsedAPIs << std::endl;
		ostr << std::left << std::setw(24) << "XPUInfo API Version: " << XPUINFO_API_VERSION_STRING << 
			", client build=\"" << m_clientBuildTimestamp << "\", internal build=\"" << m_internalBuildTimestamp << "\"\n";
	}
}

std::ostream& operator<<(std::ostream& ostr, const XPUInfo& xi)
{
	xi.printInfo(ostr);
	return ostr;
}

DXCoreAdapterMemoryBudget XI::Device::getMemUsage() const
{
	DXCoreAdapterMemoryBudget memUsage{};

#if __APPLE__
    return getMemUsage_Metal();
#else

#ifdef XPUINFO_USE_DXCORE
	if (XPUInfo::hasDXCore())
	{
		return getMemUsage_DXCORE();
	}
#endif
#endif
        
	return memUsage;
}

std::ostream& operator<<(std::ostream& ostr, const ConstDevicePtrVec& devPtrs)
{
	ostr << devPtrs.m_label << " (" << devPtrs.size() << "):\n";
	for (int i = 0; i < (int)devPtrs.size(); ++i)
	{
		ostr << "\t" << i << ": " << XI::convert(devPtrs[i]->name()) << " (0x" << std::hex << devPtrs[i]->getLUID() << std::dec << ")" << std::endl;
	}
	return ostr;
}

std::mutex ScopedRegisterNotification::m_NotificationMutex;

ScopedRegisterNotification::ScopedRegisterNotification(UI64 deviceLUID, const XPUInfo* pXI, TypeFlags flags, const DXCoreNotificationFunc& callbackFunc) : 
	m_pXI(pXI), m_NotificationFunc(callbackFunc), m_flags(flags)
{
#ifdef XPUINFO_USE_DXCORE
	if (flags && XPUInfo::hasDXCore()) // currently only for DXCore
	{
		register_DXCORE(deviceLUID);
	}
#endif
}

ScopedRegisterNotification::~ScopedRegisterNotification() noexcept(false)
{
#ifdef XPUINFO_USE_DXCORE
	if ((m_flags) && XPUInfo::hasDXCore()) // currently only for DXCore
	{
		unregister_DXCORE();
	}
#endif
}

#if defined(_WIN32)
void ScopedRegisterNotification::ExampleNotificationFunc(DXCoreNotificationType notificationType, IUnknown* object, const XPUInfo* pXI)
{
#ifdef XPUINFO_USE_DXCORE
	if (XPUInfo::hasDXCore())
	{
		ExampleNotificationFunc_DXCORE(notificationType, object, pXI);
	}
#endif
}
#else
void ScopedRegisterNotification::ExampleNotificationFunc() {}
#endif

const DeviceCPU& XPUInfo::getCPUDevice() const
{
	XPUINFO_REQUIRE(!!m_pCPU);
	return *m_pCPU;
}

#ifdef XPUINFO_USE_SYSTEMEMORYINFO
size_t SystemMemoryInfo::getCurrentAvailablePhysicalMemory()
{
#ifdef _WIN32
	PERFORMANCE_INFORMATION pi;
	XPUINFO_REQUIRE(GetPerformanceInfo(&pi, sizeof(pi)));
	return pi.PhysicalAvailable * pi.PageSize;
#elif defined(__APPLE__)
	mach_port_t            hostPort;
	mach_msg_type_number_t hostSize;
	vm_size_t              pageSize;

	hostPort = mach_host_self();
	hostSize = sizeof(vm_statistics64_data_t) / sizeof(integer_t);
	host_page_size(hostPort, &pageSize);

	vm_statistics64_data_t vmStat;

	XPUINFO_REQUIRE(host_statistics(hostPort, HOST_VM_INFO, (host_info_t)&vmStat, &hostSize) == KERN_SUCCESS);
	return vmStat.free_count * pageSize;
#elif defined(__linux__)
	long availablePages = sysconf(_SC_AVPHYS_PAGES);
	XPUINFO_REQUIRE(availablePages != -1);
	long pagesize = sysconf(_SC_PAGESIZE);
	XPUINFO_REQUIRE(pagesize != -1);

	return availablePages * pagesize;
#else
	return 0;
#endif
}

size_t SystemMemoryInfo::getCurrentTotalPhysicalMemory()
{
#ifdef _WIN32
	PERFORMANCE_INFORMATION pi;
	XPUINFO_REQUIRE(GetPerformanceInfo(&pi, sizeof(pi)));
	return pi.PhysicalTotal * pi.PageSize;
#else
	return 0;
#endif
}

size_t SystemMemoryInfo::getCurrentInstalledPhysicalMemory()
{
#ifdef _WIN32
	ULONGLONG TotalMemoryInKilobytes = 0;
	BOOL bSuccess = GetPhysicallyInstalledSystemMemory(&TotalMemoryInKilobytes);
	if (bSuccess)
	{
		return TotalMemoryInKilobytes * 1024ULL;
	}
	else
	{
		 return 0;
	}
#elif defined(__APPLE__)
	uint64_t ram = 0;
	auto size = sizeof(ram);
	XPUINFO_REQUIRE(!sysctlbyname("hw.memsize", &ram, &size, NULL, 0));
	return ram;
#elif defined(__linux__)
	struct sysinfo info;
	XPUINFO_REQUIRE(!sysinfo(&info));
	return info.totalram;
#else
	return 0;
#endif
}


SystemMemoryInfo::SystemMemoryInfo()
{
#ifdef _WIN32
	PERFORMANCE_INFORMATION pi;
	XPUINFO_REQUIRE(GetPerformanceInfo(&pi, sizeof(pi)));
	m_totalPhysicalMemory = pi.PhysicalTotal * pi.PageSize;
	m_availablePhysicalMemoryAtInit = pi.PhysicalAvailable * pi.PageSize;
	m_pageSize = pi.PageSize;
#elif defined(__APPLE__) || defined(__linux__)

#if defined(__APPLE__)
	vm_size_t              pageSize;
	host_page_size(mach_host_self(), &pageSize);
	m_pageSize = pageSize;
#elif defined(__linux__)
	long sysconfResult = sysconf(_SC_PAGESIZE);
	XPUINFO_REQUIRE(sysconfResult != -1);
	m_pageSize = sysconfResult;
#endif

	m_totalPhysicalMemory = getCurrentTotalPhysicalMemory();
	m_availablePhysicalMemoryAtInit = getCurrentAvailablePhysicalMemory();
#endif
	m_installedPhysicalMemory = getCurrentInstalledPhysicalMemory();

	if (m_totalPhysicalMemory == 0)
	{
		m_totalPhysicalMemory = m_installedPhysicalMemory;
	}
}
#endif // XPUINFO_USE_SYSTEMEMORYINFO

} // XI

#if defined(_WIN32) && defined(XPUINFO_BUILD_SHARED)
extern "C" BOOL WINAPI DllMain(
	HINSTANCE const /*instance*/,  // handle to DLL module
	DWORD     const reason,    // reason for calling function
	LPVOID    const )  // reserved
{
	// Perform actions based on the reason for calling.
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		// Initialize once for each new process.
		// Return FALSE to fail DLL load.
		break;

	case DLL_THREAD_ATTACH:
		// Do thread-specific initialization.
		break;

	case DLL_THREAD_DETACH:
		// Do thread-specific cleanup.
		break;

	case DLL_PROCESS_DETACH:
		// Perform any necessary cleanup.
		break;
	}
	return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
#endif
