// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#include <d3d11_4.h>
#include <wrl/client.h>
#include "DebugStream.h"

namespace WRL = Microsoft::WRL;

// Adapted from Intel GPUDetect code sample

#define GGF_SUCCESS 0
#define GGF_ERROR					-1
#define GGF_E_UNSUPPORTED_HARDWARE	-2
#define GGF_E_UNSUPPORTED_DRIVER	-3
#define GGF_E_D3D_ERROR				-4

// The new device dependent counter
#define INTEL_VENDOR_ID 0x8086

// The new device dependent counter
#define INTEL_DEVICE_INFO_COUNTERS         "Intel Device Information"

// From DXUT.h
#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p)=nullptr; } }
#endif    
#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p)=nullptr; } }
#endif    
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=nullptr; } }
#endif
#define DXTRACE_ERR(msg, hr) { XI::DebugStreamT dStr(false); dStr << (msg) << ": " << (hr) << std::endl; }

namespace
{
	// New device dependent structure
	struct IntelDeviceInfoV1
	{
		DWORD GPUMaxFreq;
		DWORD GPUMinFreq;
	};

	struct IntelDeviceInfoV2
	{
		DWORD GPUMaxFreq;
		DWORD GPUMinFreq;
		DWORD GTGeneration;
		DWORD EUCount;
		DWORD PackageTDP;
		DWORD MaxFillRate;
	};

	struct IntelDeviceInfoHeader
	{
		DWORD Size;
		DWORD Version;
	};

	/*****************************************************************************************
* checkForIntelCounter
*
* Description:
*       Checks a device counter for match to INTEL_DEVICE_INFO_COUNTERS
*           Supported device info:
*                GPU Max Frequency, GPU Min Frequency, GT Generation, EU Count, Package TDP, Max Fill Rate
*
* Parameters:
*         ID3D11Device				*pDevice						- Input:	pointer to graphics device object
*
*         int						index							- Input:	index to counter to check
*
*         D3D11_COUNTER_DESC		&intelCounterDesc				- Input:	allocated counter descriptor struct
*																	  Output:	the counter found, if matches
*
*         IntelDeviceInfoHeader		*pIntelDeviceInfoHeader			- Input:	allocated Intel Device Info Header
*																	  Output:	Info header filled with Version and Size
*
* Return:
*         true:		found the Intel device info counter
*         false:	not found
*
*****************************************************************************************/

	bool	checkForIntelCounter(ID3D11Device* pDevice, int index, D3D11_COUNTER_DESC& intelCounterDesc, IntelDeviceInfoHeader* pIntelDeviceInfoHeader)
	{
		bool	bRetVal = false;	// default to failed
		HRESULT hr = NULL;

		D3D11_COUNTER_TYPE counterType;
		D3D11_COUNTER_DESC counterDescription;
		UINT uiSlotsRequired, uiNameLength, uiUnitsLength, uiDescLength;

		counterType = static_cast<D3D11_COUNTER_TYPE>(0);
		counterDescription.Counter = static_cast<D3D11_COUNTER>(index + D3D11_COUNTER_DEVICE_DEPENDENT_0);
		counterDescription.MiscFlags = 0;
		uiSlotsRequired = uiNameLength = uiUnitsLength = uiDescLength = 0;

		// Obtain string sizes CheckCounter needs to return
		hr = pDevice->CheckCounter(&counterDescription, &counterType, &uiSlotsRequired, nullptr, &uiNameLength, nullptr, &uiUnitsLength, nullptr, &uiDescLength);

		if (SUCCEEDED(hr))
		{
			// CREATE SPACE FOR COUNTER STRINGS
			LPSTR sName = new char[uiNameLength];
			LPSTR sUnits = new char[uiUnitsLength];
			LPSTR sDesc = new char[uiDescLength];

			// obtain the strings from counter - will use sDesc and sUnits
			hr = pDevice->CheckCounter(&counterDescription, &counterType, &uiSlotsRequired, sName, &uiNameLength, sUnits, &uiUnitsLength, sDesc, &uiDescLength);

			if (SUCCEEDED(hr))
			{
				int	match = strcmp(sName, INTEL_DEVICE_INFO_COUNTERS);

				if (match == 0)
				{
					int IntelCounterMajorVersion;
					int IntelCounterSize;
					int argsFilled;

					// Save counter to return 
					intelCounterDesc.Counter = counterDescription.Counter;

					argsFilled = sscanf_s(sDesc, "Version %d", &IntelCounterMajorVersion);

					//	If sscanf extracted one field (Version), must not be Version 1 of Intel counter description string
					if (argsFilled == 1)
					{
						argsFilled = sscanf_s(sUnits, "Size %d", &IntelCounterSize);
						if (!argsFilled)
						{
							if (IntelCounterMajorVersion == 2)
							{
								IntelCounterSize = sizeof(IntelDeviceInfoV2);
							}
						}
					}
					else
					{
						//	Fall back to version 1.0 - assume at least that is supported
						IntelCounterMajorVersion = 1;
						IntelCounterSize = sizeof(IntelDeviceInfoV1);
					}

					// save version/size for return
					pIntelDeviceInfoHeader->Version = IntelCounterMajorVersion;
					pIntelDeviceInfoHeader->Size = IntelCounterSize;

					bRetVal = true;		// Success

				}
			}

			// DELETE STRING SPACE - SIZE MAY DIFFER FOR NEXT COUNTER
			SAFE_DELETE_ARRAY(sName);
			SAFE_DELETE_ARRAY(sUnits);
			SAFE_DELETE_ARRAY(sDesc);

		}

		return	bRetVal;

	}

	/*****************************************************************************************
	 * getIntelDeviceInfo
	 *
	 * Description:
	 *       Gets device info if available
	 *           Supported device info:
	 *                GPU Max Frequency, GPU Min Frequency, GT Generation, EU Count, Package TDP, Max Fill Rate
	 *
	 * Parameters:
	 *         IntelDeviceInfoHeader *pIntelDeviceInfoHeader -        Input:  allocated IntelDeviceInfoHeader *
	 *                                                                Output: Intel device info header, if found
	 *
	 *         void *pIntelDeviceInfoBuffer                   -       Input:  allocated void * with sufficient space
	 *                                                                          for largest counter data
	 *                                                                Output: IntelDeviceInfoV[#] counter data
	 *                                                                          based on IntelDeviceInfoHeader
	 * Return:
	 *         GGF_SUCCESS: Able to find Data is valid
	 *         GGF_ERROR:   General error.
	 *         GGF_E_UNSUPPORTED_DRIVER: Unsupported driver on Intel, data is invalid
	 *         GGF_E_D3D_ERROR:   General error return - unkonwn D3D failure.
	 *
	 *****************************************************************************************/

	long getIntelDeviceInfo(IntelDeviceInfoHeader* pIntelDeviceInfoHeader, void* pIntelDeviceInfoBuffer, IDXGIAdapter1* pIntelAdapter)
	{
		XPUINFO_REQUIRE(pIntelDeviceInfoBuffer != nullptr);
		XPUINFO_REQUIRE(pIntelDeviceInfoHeader);
		if (!pIntelDeviceInfoBuffer || !pIntelDeviceInfoHeader)
		{
			return GGF_ERROR;
		}

		// The device information is stored in a D3D counter.
		// We must create a D3D device, find the Intel counter 
		// and query the counter info
		HRESULT				hr = NULL;
		ID3D11Device* pDevice = nullptr;
		ID3D11DeviceContext* pImmediateContext = nullptr;

		D3D_FEATURE_LEVEL	featureLevel;
		ZeroMemory(&featureLevel, sizeof(D3D_FEATURE_LEVEL));

		// Create the D3D11 Device for primary graphics
		hr = D3D11CreateDevice(pIntelAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
			D3D11_SDK_VERSION, &pDevice, &featureLevel, &pImmediateContext);

		if (FAILED(hr))
		{
			SAFE_RELEASE(pImmediateContext);
			SAFE_RELEASE(pDevice);

			XI::DebugStream dStr(true);
			dStr << _XI_FILE_ << ": " << __FUNCTION__ << ": D3D11CreateDevice failed: " << hr << std::endl;

			return GGF_E_D3D_ERROR;
		}

		// The counter is in a device dependent counter
		D3D11_COUNTER_INFO	counterInfo;
		ZeroMemory(&counterInfo, sizeof(D3D11_COUNTER_INFO));

		// Query the device to find the number of device dependent counters.
		pDevice->CheckCounterInfo(&counterInfo);

		if (counterInfo.LastDeviceDependentCounter == 0)
		{
			SAFE_RELEASE(pImmediateContext);
			SAFE_RELEASE(pDevice);

			//DXTRACE_ERR(TEXT("No device dependent counters"), hr);

			// The driver does not support the Device Info Counter.
			return GGF_E_UNSUPPORTED_DRIVER;
		}

		// Get number of device dependent counters to search through
		int		numDependentCounters = counterInfo.LastDeviceDependentCounter - D3D11_COUNTER_DEVICE_DEPENDENT_0 + 1;

		// The counter is in a device dependent counter
		// Search for the Intel device specific counter by name -  INTEL_DEVICE_INFO_COUNTERS
		D3D11_COUNTER_DESC	intelCounterDesc;
		ZeroMemory(&intelCounterDesc, sizeof(D3D11_COUNTER_DESC));

		// Search device dependent counters for INTEL_DEVICE_INFO_COUNTERS
		for (int i = 0; i < numDependentCounters; ++i)
		{
			if (checkForIntelCounter(pDevice, i, intelCounterDesc, pIntelDeviceInfoHeader))
				break;
		}

		// Make sure found Intel counter description
		if (intelCounterDesc.Counter == 0)
		{
			SAFE_RELEASE(pImmediateContext);
			SAFE_RELEASE(pDevice);

			DXTRACE_ERR(TEXT("Could not find counter"), hr);

			// The driver does not support the Intel Device Info Counter.
			return GGF_E_UNSUPPORTED_DRIVER;
		}



		// Create the Intel device counter
		ID3D11Counter* pIntelCounter = nullptr;

		hr = pDevice->CreateCounter(&intelCounterDesc, &pIntelCounter);
		if (FAILED(hr))
		{
			SAFE_RELEASE(pIntelCounter);
			SAFE_RELEASE(pImmediateContext);
			SAFE_RELEASE(pDevice);

			DXTRACE_ERR(TEXT("CreateCounter failed"), hr);
			return GGF_E_D3D_ERROR;
		}


		// Begin and end counter capture to collect data
		pImmediateContext->Begin(pIntelCounter);
		pImmediateContext->End(pIntelCounter);



		// Create space for a pointer to counter data buffer
		DWORD pData[2] = { 0,0 };		//  Can hold a 32 or 64 bit pointer

		// Retrieve a pointer to Intel counter data - NOTE: NOT the data itself!
		hr = pImmediateContext->GetData(pIntelCounter, pData, sizeof(pData), 0);

		if (FAILED(hr) || hr == S_FALSE)
		{
			SAFE_RELEASE(pIntelCounter);
			SAFE_RELEASE(pImmediateContext);
			SAFE_RELEASE(pDevice);

			DXTRACE_ERR(TEXT("GetData failed"), hr);
			return GGF_E_D3D_ERROR;
		}


		// Extract returned pointer to counter data buffer
		void* pDeviceInfoBuffer = *(void**)pData;

		// Copy Counter data to save in buffer space passed on pIntelDeviceInfoBuffer
		memcpy(pIntelDeviceInfoBuffer, pDeviceInfoBuffer, pIntelDeviceInfoHeader->Size);


		// Clean up
		SAFE_RELEASE(pIntelCounter);
		SAFE_RELEASE(pImmediateContext);
		SAFE_RELEASE(pDevice);

		return GGF_SUCCESS;
	}
} // private

namespace XI
{
void Device::initDXIntelPerfCounter(IDXGIAdapter1* pAdapter)
{
	// Retrieve Intel device information
	IntelDeviceInfoHeader	intelDeviceInfoHeader = { 0 };
	byte					intelDeviceInfoBuffer[1024];	// enough space to allow some future expansion

	DebugStream dStr(false);
	long getStatus = getIntelDeviceInfo(&intelDeviceInfoHeader, &intelDeviceInfoBuffer, pAdapter);
	if (getStatus == GGF_SUCCESS)
	{
		//_tprintf(TEXT("Intel Device Info Version  %d (%d bytes)\n"), intelDeviceInfoHeader.Version, intelDeviceInfoHeader.Size);

		if (intelDeviceInfoHeader.Version >= 2)
		{
			IntelDeviceInfoV2* pIntelDeviceInfo = (IntelDeviceInfoV2*)intelDeviceInfoBuffer;
			updateIfDstNotSet(m_props.FreqMaxMHz, (I32)pIntelDeviceInfo->GPUMaxFreq);
			updateIfDstNotSet(m_props.FreqMinMHz, (I32)pIntelDeviceInfo->GPUMinFreq);
			updateIfDstNotSet(m_props.DeviceGenerationID, (I32)pIntelDeviceInfo->GTGeneration);
			updateIfDstVal(m_props.DeviceGenerationAPI, API_TYPE_UNKNOWN, API_TYPE_DX11_INTEL_PERF_COUNTER);
			updateIfDstNotSet(m_props.NumComputeUnits, (I32)pIntelDeviceInfo->EUCount);
			if (pIntelDeviceInfo->PackageTDP > 0)
				updateIfDstNotSet(m_props.PackageTDP, (I32)pIntelDeviceInfo->PackageTDP);
			// MaxFillRate needed?

			if (intelDeviceInfoHeader.Version > 2)
			{
				dStr << __FUNCTION__ << ": NOTE: DeviceInfoHeader.Version > 2, check for updated fields\n";
			}
			validAPIs = validAPIs | API_TYPE_DX11_INTEL_PERF_COUNTER;
		}
		else if (intelDeviceInfoHeader.Version == 1)
		{
			IntelDeviceInfoV1* pIntelDeviceInfo = (IntelDeviceInfoV1*)intelDeviceInfoBuffer;
			updateIfDstNotSet(m_props.FreqMaxMHz, (I32)pIntelDeviceInfo->GPUMaxFreq);
			updateIfDstNotSet(m_props.FreqMinMHz, (I32)pIntelDeviceInfo->GPUMinFreq);
			validAPIs = validAPIs | API_TYPE_DX11_INTEL_PERF_COUNTER;
		}
		else
		{
			dStr << __FUNCTION__ << "ERROR: UNKNOWN Intel Device Version \n";
		}
	}
	else if (getStatus == GGF_E_UNSUPPORTED_HARDWARE)
	{
		dStr << __FUNCTION__ << "ERROR: GGF_E_UNSUPPORTED_HARDWARE\n";
	}
	else if (getStatus == GGF_E_UNSUPPORTED_DRIVER)
	{
		dStr << __FUNCTION__ << "ERROR: GGF_E_UNSUPPORTED_DRIVER\n";
	}
	else
	{
		dStr << __FUNCTION__ << "ERROR: UNKOWN ERROR\n";
	}
}

} // XI
