// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_SETUPAPI

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#endif
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#include <initguid.h>   // include before devguid.h, devpkey.h in one module
#include <devguid.h>
#include <Devpkey.h>
#include <SetupAPI.h>
#include <sstream>
#include "DebugStream.h"

#pragma comment (lib, "Setupapi.lib")

#ifndef XPUINFO_USE_D3DKMT
#define XPUINFO_USE_D3DKMT 1
#endif

#if XPUINFO_USE_D3DKMT
#ifndef XPUINFO_USE_D3DKMT_VERBOSE
#ifdef _DEBUG
#define XPUINFO_USE_D3DKMT_VERBOSE 1
#else
#define XPUINFO_USE_D3DKMT_VERBOSE 0
#endif // _DEBUG
#endif

#ifndef NTSTATUS
#define NTSTATUS LONG
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#include <d3dkmthk.h> // From Windows Driver Kit (WDK) or Windows SDK
using PFN_D3DKMTOpenAdapterFromLuid = NTSTATUS(APIENTRY*)(D3DKMT_OPENADAPTERFROMLUID*);
using PFN_D3DKMTQueryAdapterInfo = NTSTATUS(APIENTRY*)(const D3DKMT_QUERYADAPTERINFO*);
using PFN_D3DKMTCloseAdapter = NTSTATUS(APIENTRY*)(const D3DKMT_CLOSEADAPTER*);
#endif // XPUINFO_USE_D3DKMT

namespace
{
    // Found here, but not sure where they got it: https://wine-devel.winehq.narkive.com/vKO2Bkgj/patch-1-2-dxgi-tests-add-test-for-enumerating-display-adapters-using-setupapi
    // Valid on >=Win10
    DEFINE_DEVPROPKEY(DEVPROPKEY_DISPLAY_ADAPTER_LUID, 0x60b193cb, 0x5276, 0x4d0f, 0x96, 0xfc, 0xf1, 0x73, 0xab, 0xad, 0x3e, 0xc6, 2); // sizeof(LUID)

	/*
	Return code	Description
	ERROR_INVALID_FLAGS
	The value of Flags is not zero.
	ERROR_INVALID_HANDLE
	The device information set that is specified by DevInfoSet is not valid.
	ERROR_INVALID_PARAMETER
	A supplied parameter is not valid. One possibility is that the device information element is not valid.
	ERROR_INVALID_REG_PROPERTY
	The property key that is supplied by PropertyKey is not valid.
	ERROR_INVALID_DATA
	An unspecified internal data value was not valid.
	ERROR_INVALID_USER_BUFFER
	A user buffer is not valid. One possibility is that PropertyBuffer is NULL and PropertBufferSize is not zero.
	ERROR_NO_SUCH_DEVINST
	The device instance that is specified by DevInfoData does not exist.
	ERROR_INSUFFICIENT_BUFFER
	The PropertyBuffer buffer is too small to hold the requested property value, or an internal data buffer that was passed to a system call was too small.
	ERROR_NOT_ENOUGH_MEMORY
	There was not enough system memory available to complete the operation.
	ERROR_NOT_FOUND
	The requested device property does not exist.
	ERROR_ACCESS_DENIED
	The caller does not have Administrator privileges.
	*/

#define CASE_ERR_TO_STR(e) case e: str << #e; break;

	XI::WString GetErrorCodeStr(DWORD lr)
	{
		std::wostringstream str;
		switch (lr)
		{
			CASE_ERR_TO_STR(ERROR_INVALID_FLAGS)
			CASE_ERR_TO_STR(ERROR_INVALID_HANDLE)
			CASE_ERR_TO_STR(ERROR_INVALID_PARAMETER)
			CASE_ERR_TO_STR(ERROR_INVALID_REG_PROPERTY)
			CASE_ERR_TO_STR(ERROR_INVALID_DATA)
			CASE_ERR_TO_STR(ERROR_INVALID_USER_BUFFER)
			CASE_ERR_TO_STR(ERROR_NO_SUCH_DEVINST)
			CASE_ERR_TO_STR(ERROR_INSUFFICIENT_BUFFER)
			CASE_ERR_TO_STR(ERROR_NOT_ENOUGH_MEMORY)
			CASE_ERR_TO_STR(ERROR_NOT_FOUND)
			CASE_ERR_TO_STR(ERROR_ACCESS_DENIED)
		default:
			str << L"Unknown Error";
		}

		return str.str();
	}

	bool sdiGetProp(HDEVINFO info, std::vector<wchar_t>& tempBuf, PSP_DEVINFO_DATA pDID, const DEVPROPKEY* pDPK, XI::WString& outStr)
	{
		DEVPROPTYPE PropType;
		bool bRet = SetupDiGetDevicePropertyW(
			info,
			pDID,
			pDPK,
			&PropType,
			(PBYTE)tempBuf.data(),
			(DWORD)tempBuf.size(),
			nullptr,
			0);
		if (!bRet)
		{
			auto lastErr = GetLastError();
			XI::DebugStreamW dStr(true);
			dStr << L"ERROR: " << __FUNCTIONW__ << L" returned " << GetErrorCodeStr(lastErr) << "(" << lastErr << ")\n";
		}
		bRet = bRet && (PropType == DEVPROP_TYPE_STRING);
		if (bRet)
		{
			outStr = tempBuf.data();
		}
		return bRet;
	}

#if XPUINFO_USE_D3DKMT
	bool GetAdapterPciAddress_D3DKMT(
		LUID adapterLuid, XI::PCIAddressType& outPciAddr)
	{
        bool gdi32Loaded = false;
		HMODULE hGdi32 = GetModuleHandleW(L"gdi32.dll");
		if (!hGdi32)
		{
			hGdi32 = LoadLibraryW(L"gdi32.dll");
			gdi32Loaded = true;
		}
		if (!hGdi32) return false;

		auto pfnOpen = (PFN_D3DKMTOpenAdapterFromLuid)GetProcAddress(hGdi32, "D3DKMTOpenAdapterFromLuid");
		auto pfnQuery = (PFN_D3DKMTQueryAdapterInfo)GetProcAddress(hGdi32, "D3DKMTQueryAdapterInfo");
		auto pfnClose = (PFN_D3DKMTCloseAdapter)GetProcAddress(hGdi32, "D3DKMTCloseAdapter");
		if (!pfnOpen || !pfnQuery || !pfnClose) return false;

		D3DKMT_OPENADAPTERFROMLUID openInfo = { adapterLuid, 0 };
		if (pfnOpen(&openInfo) != STATUS_SUCCESS) return false;

		D3DKMT_ADAPTERADDRESS addr = {};
		D3DKMT_QUERYADAPTERINFO queryInfo = {};
		queryInfo.hAdapter = openInfo.hAdapter;
		// See https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ne-d3dkmthk-_kmtqueryadapterinfotype
		queryInfo.Type = KMTQAITYPE_ADAPTERADDRESS;
		queryInfo.pPrivateDriverData = &addr;
		queryInfo.PrivateDriverDataSize = sizeof(addr);

		NTSTATUS status = pfnQuery(&queryInfo);

		D3DKMT_CLOSEADAPTER closeInfo = { openInfo.hAdapter };
		pfnClose(&closeInfo);

		if (gdi32Loaded)
		{
			FreeLibrary(hGdi32);
		}
		if (status != STATUS_SUCCESS) return false;

		outPciAddr.domain = (addr.BusNumber >> 8) & 0xFFFF;
		outPciAddr.bus = addr.BusNumber & 0xFF;
		outPciAddr.device = addr.DeviceNumber;
		outPciAddr.function = addr.FunctionNumber;
		return true;
	}
#endif // XPUINFO_USE_D3DKMT

	bool QueryDevicePciAddress(HDEVINFO devInfo, SP_DEVINFO_DATA* devData, XI::PCIAddressType& outAddr) {
		DEVPROPTYPE propType = 0;
		UINT32 rawBusNumber = 0;
		DWORD requiredSize = 0;

		// 1. Query DEVPKEY_Device_BusNumber (Type: DEVPROP_TYPE_UINT32)
		if (!SetupDiGetDevicePropertyW(
			devInfo,
			devData,
			&DEVPKEY_Device_BusNumber,
			&propType,
			reinterpret_cast<PBYTE>(&rawBusNumber),
			sizeof(rawBusNumber),
			&requiredSize,
			0) || propType != DEVPROP_TYPE_UINT32) {
			return false;
		}

		// 2. Query DEVPKEY_Device_Address (Type: DEVPROP_TYPE_UINT32)
		UINT32 rawAddress = 0;
		if (!SetupDiGetDevicePropertyW(
			devInfo,
			devData,
			&DEVPKEY_Device_Address,
			&propType,
			reinterpret_cast<PBYTE>(&rawAddress),
			sizeof(rawAddress),
			&requiredSize,
			0) || propType != DEVPROP_TYPE_UINT32) {
			return false;
		}

		// Windows pci.sys packs the 16-bit PCI Segment (Domain) in bits [23:8]
		// and the 8-bit PCI Bus Number in bits [7:0] of DEVPKEY_Device_BusNumber.
		outAddr.domain = (rawBusNumber >> 8) & 0xFFFF;
		outAddr.bus = rawBusNumber & 0xFF;

		// DEVPKEY_Device_Address packs the 16-bit Device (Slot) in the upper word
		// and the 16-bit Function in the lower word.
		outAddr.device = (rawAddress >> 16) & 0xFFFF;
		outAddr.function = rawAddress & 0xFFFF;

		return true;
	}
} // private

namespace XI
{
namespace //private
{
	bool getInfoForClass(const GUID* devClass, std::vector<DriverInfoPtr>& outInfos)
	{
		// We need to include all Enumerator classes, 
		//   then filter out non-GPU devices under SWD or USB
		HDEVINFO m_info = SetupDiGetClassDevsW(devClass,
			nullptr, nullptr, DIGCF_PRESENT);
		HYBRIDDETECT_DEBUG_REQUIRE(m_info != INVALID_HANDLE_VALUE);

		SP_DEVINFO_DATA devInfoData;
		ZeroMemory(&devInfoData, sizeof(devInfoData));
		devInfoData.cbSize = sizeof(devInfoData);

		DWORD devIdx = 0;
		while (SetupDiEnumDeviceInfo(m_info, devIdx, &devInfoData))
		{
			DebugStreamW dStr(false);
			DriverInfoPtr curInfo(new DriverInfo);

			dStr << L"\tDevice " << devIdx << ": ";
			++devIdx;
			DWORD numKeys = 0;
			if (SetupDiGetDevicePropertyKeys(m_info, &devInfoData, nullptr, 0, &numKeys, 0))
			{
				dStr << L" with " << numKeys << L" keys";
			}

			DWORD nameSize = 0;
			std::vector<wchar_t> tempStr;
			tempStr.resize(256);

			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_DriverDesc, curInfo->DriverDesc))
			{
				dStr << curInfo->DriverDesc;
			}

			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_DeviceDesc, curInfo->DeviceDesc))
			{
				if (curInfo->DeviceDesc != curInfo->DriverDesc)
				{
					dStr << L", (" << curInfo->DeviceDesc << ")";
				}
			}
			else
			{
				DWORD err = GetLastError();
				dStr << L",(DEVPKEY_Device_DeviceDesc: " << GetErrorCodeStr(err) << ")";
			}

			/* If you are iterating via SetupDiGetClassDevs, pull the DEVPKEY_Device_EnumeratorName property. 
			   If the class is GUID_DEVCLASS_DISPLAY but the enumerator string isn't "PCI", you can confidently 
			   flag it as a virtual adapter.*/
			XI::WString& enumeratorName = curInfo->EnumeratorName;
			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_EnumeratorName, enumeratorName))
			{
				dStr << L", " << enumeratorName;
				if (enumeratorName == L"SWD")
				{
					dStr << L", SKIPPED\n";
					continue; // Skip software devices like "Microsoft Basic Render Driver"
				}
			}
			else
			{
				DWORD err = GetLastError();
				dStr << L",(DEVPKEY_Device_EnumeratorName: " << GetErrorCodeStr(err) << ")";
			}

			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_DriverVersion, curInfo->DriverVersion))
			{
				dStr << L", " << curInfo->DriverVersion;
			}

			DEVPROPTYPE PropType;
			FILETIME fileTime{};
			SYSTEMTIME sysTime{};
			if (SetupDiGetDevicePropertyW(
				m_info,
				&devInfoData,
				&DEVPKEY_Device_DriverDate,
				&PropType,
				(PBYTE)&fileTime,
				sizeof(FILETIME),
				nullptr,
				0))
			{
				curInfo->DriverDate = fileTime;
				SYSTEMTIME curSysTime;
				GetSystemTime(&curSysTime);
				if (FileTimeToSystemTime(&fileTime, &sysTime))
				{
					dStr << L", " << sysTime.wMonth << L"/" << sysTime.wDay << L"/" << sysTime.wYear;
					float yearsCur = curSysTime.wYear + curSysTime.wMonth / 12.0f + curSysTime.wDay / 365.25f;
					float yearsDriver = sysTime.wYear + sysTime.wMonth / 12.0f + sysTime.wDay / 365.25f;
					auto oldPrec = dStr.precision(2);
					dStr << L" (" << yearsCur - yearsDriver << " years old)";
					dStr.precision(oldPrec);
				}
			}

			if (SetupDiGetDevicePropertyW(
				m_info,
				&devInfoData,
				&DEVPKEY_Device_InstallDate,
				&PropType,
				(PBYTE)&fileTime,
				sizeof(FILETIME),
				nullptr,
				0))
			{
				curInfo->InstallDate = fileTime;
				SYSTEMTIME curSysTime;
				GetSystemTime(&curSysTime);
				if (FileTimeToSystemTime(&fileTime, &sysTime))
				{
					dStr << L",InstallDate, " << sysTime.wMonth << L"/" << sysTime.wDay << L"/" << sysTime.wYear;
					float yearsCur = curSysTime.wYear + curSysTime.wMonth / 12.0f + curSysTime.wDay / 365.25f;
					float yearsDriver = sysTime.wYear + sysTime.wMonth / 12.0f + sysTime.wDay / 365.25f;
					auto oldPrec = dStr.precision(2);
					dStr << L" (" << yearsCur - yearsDriver << " years old)";
					dStr.precision(oldPrec);
				}
			}

			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_InstanceId, curInfo->DeviceInstanceId))
			{
				dStr << L"(" << tempStr.data() << L") ";
			}

			XI::WString& deviceService = curInfo->DeviceService;
			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_Service, deviceService))
			{
				dStr << L", service = \"" << deviceService << L"\"";
			}

			// For Intel, this might be an unofficial way to find the "GT Generation"
			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_DriverInfSection, curInfo->DriverInfSection))
			{
				dStr << ", Inf Section = " << curInfo->DriverInfSection;
			}

			LUID curLUID{};
			HYBRIDDETECT_DEBUG_REQUIRE(LuidToUI64(curInfo->DeviceLUID) == 0ULL);
			if (SetupDiGetDevicePropertyW(
				m_info,
				&devInfoData,
				&DEVPROPKEY_DISPLAY_ADAPTER_LUID,
				&PropType,
				(PBYTE)&curLUID,
				(DWORD)sizeof(LUID),
				&nameSize,
				0) && (nameSize == sizeof(LUID)))
			{
				curInfo->DeviceLUID = curLUID;
				XI::UI64 luidUI64 = LuidToUI64(curLUID);
				dStr << ", LUID = " << std::hex << luidUI64 << std::dec;
				if (!luidUI64)
				{
					dStr << L", SKIPPED\n";
					continue;
				}
			}
			else
			{
				dStr << L", Non-Display, SKIPPED\n";
				continue;
			}

			if (enumeratorName == L"PCI")
			{
				// Software or virtual adapters (such as Microsoft Basic Render Driver or Remote Desktop Display adapters) 
				// do not have a DEVPKEY_Device_BusNumber property; SetupDiGetDevicePropertyW will return FALSE with 
				// GetLastError() == ERROR_NOT_FOUND (1168), which safely filters them out.
				bool hasPciAddr = QueryDevicePciAddress(m_info, &devInfoData, curInfo->LocationInfo);

				if (!hasPciAddr || !curInfo->LocationInfo.valid())
				{
					// Only warn here, filter out in DriverInfo::isValidXPU()
					dStr << L" ** Error querying PCI Address! Result: " << curInfo->LocationInfo << std::endl;
				}
				else
				{
					dStr << L", PCI Address = " << curInfo->LocationInfo;
#if XPUINFO_USE_D3DKMT
					// Verify SetupAPI PCI Address matches D3DKMT PCI Address for this LUID
					XI::PCIAddressType d3dkmtAddr;
					if (GetAdapterPciAddress_D3DKMT(curInfo->DeviceLUID, d3dkmtAddr))
					{
						if (!(d3dkmtAddr == curInfo->LocationInfo))
						{
							std::wostringstream errStr;
							errStr << L" ** D3DKMT PCI Address mismatch! SetupAPI: " << curInfo->LocationInfo <<
								L", D3DKMT: " << d3dkmtAddr << std::endl;
							dStr << errStr.str();
#if XPUINFO_USE_D3DKMT_VERBOSE
							std::wcerr << L"**WARNING** For Adapter \"" << curInfo->DriverDesc << L"\" LUID 0x" 
								<< std::hex << LuidToUI64(curInfo->DeviceLUID) << std::dec << L": " << errStr.str();
#endif
						}
					}
					else
					{
#if XPUINFO_USE_D3DKMT_VERBOSE
                        std::wcerr << L"**WARNING** For Adapter \"" << curInfo->DriverDesc << L"\" LUID 0x" 
							<< std::hex << LuidToUI64(curInfo->DeviceLUID) << std::dec << L": D3DKMT failed to get PCI Address!" << std::endl;
#endif
					}
#endif
				}
			}

			outInfos.emplace_back(std::move(curInfo));
			dStr << std::endl;

		} // while

		if (m_info)
		{
			SetupDiDestroyDeviceInfoList(m_info);
		}
		return true;
	}
} // private ns

SetupDeviceInfo::SetupDeviceInfo()
{
	getInfoForClass(&GUID_DEVCLASS_DISPLAY, m_DevInfoPtrs);
	getInfoForClass(&GUID_DEVCLASS_COMPUTEACCELERATOR, m_DevInfoPtrs);
}

const DriverInfoPtr SetupDeviceInfo::getByLUID(const UI64 inLUID) const
{
	for (const auto& info : m_DevInfoPtrs)
	{
		UI64 curLUID = LuidToUI64(info->DeviceLUID);
		if (curLUID && (inLUID == curLUID))
		{
			return info;
		}
	}
	return nullptr;
}

const DriverInfoPtr SetupDeviceInfo::getAtAddress(const PCIAddressType& inAddress) const
{
	for (const auto& info : m_DevInfoPtrs)
	{
		if (inAddress == info->LocationInfo)
		{
			return info;
		}
	}
	return nullptr;
}

const DriverInfoPtr SetupDeviceInfo::getByName(const WString& inName) const
{
	for (const auto& info : m_DevInfoPtrs)
	{
		if ((inName == info->DriverDesc) || (inName == info->DeviceDesc))
		{
			return info;
		}
	}
	return nullptr;
}

SetupDeviceInfo::~SetupDeviceInfo()
{
}

} // XI
#else
// Build stub so we don't have to change interface
#include "LibXPUInfo.h"
XI::SetupDeviceInfo::~SetupDeviceInfo() {}
#endif // XPUINFO_USE_SETUPAPI
