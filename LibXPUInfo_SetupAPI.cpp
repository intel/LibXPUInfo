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
}

namespace XI
{
namespace //private
{
	bool getInfoForClass(const GUID* devClass, std::vector<DriverInfoPtr>& outInfos)
	{
		HDEVINFO m_info = SetupDiGetClassDevsW(devClass,
			L"PCI", // Filter out remote desktop which is "SWD"
			nullptr, DIGCF_PRESENT);
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

			DEVPROPTYPE PropType;
			DWORD nameSize = 0;
			std::vector<wchar_t> tempStr;
			tempStr.resize(256);

			/*
			if (SetupDiGetDevicePropertyW(
				m_info,
				&devInfoData,
				&DEVPKEY_Device_EnumeratorName,
				&PropType,
				(PBYTE)tempStr.data(),
				(DWORD)tempStr.size(),
				&nameSize,
				0))
			{
				// Should be PCI unless filter above changed
				dStr << L" at \"" << tempStr.data() << L"\"";
			}
			*/

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

			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_DriverVersion, curInfo->DriverVersion))
			{
				dStr << L", " << curInfo->DriverVersion;
			}

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

			/*
			if (SetupDiGetDevicePropertyW(
				m_info,
				&devInfoData,
				&DEVPKEY_Device_Service,
				&PropType,
				(PBYTE)tempStr.data(),
				(DWORD)tempStr.size(),
				&nameSize,
				0))
			{
				// With filter as PCI, should never be "SWD" or other indicator of software service
				dStr << L" service =  \"" << tempStr.data() << L"\"";
			}
			*/

			if (SetupDiGetDevicePropertyW(
				m_info,
				&devInfoData,
				&DEVPKEY_Device_LocationInfo,
				&PropType,
				(PBYTE)tempStr.data(),
				(DWORD)tempStr.size(),
				&nameSize,
				0))
			{
				dStr << L" at \"" << tempStr.data() << L"\"";
				bool locValid = curInfo->LocationInfo.GetFromWStr(tempStr.data());
				if (!locValid)
				{
					dStr << " ** Error parsing location!\n";
					continue;
				}
			}

			// For Intel, this might be an unofficial way to find the "GT Generation"
			if (sdiGetProp(m_info, tempStr, &devInfoData, &DEVPKEY_Device_DriverInfSection, curInfo->DriverInfSection))
			{
				dStr << ", Inf Section = " << curInfo->DriverInfSection;
			}

			LUID curLUID{};
			HYBRIDDETECT_DEBUG_REQUIRE(*(UI64*)&curInfo->DeviceLUID == 0ULL);
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
				dStr << ", LUID = " << std::hex << *(UI64*)&curLUID << std::dec;
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
