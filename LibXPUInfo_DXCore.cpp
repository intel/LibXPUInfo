// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifdef XPUINFO_USE_DXCORE

#include <initguid.h> // Must be first for <dxcore.h>
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#include "DebugStream.h"
#include <delayimp.h>
#include <iomanip>

// GPU/NPU Detection
// https://github.com/microsoft/Windows-Machine-Learning/blob/master/Tools/WinMLRunner/src/LearningModelDeviceHelper.cpp#L100-L219
#include <unknwn.h>
#include <roerrorapi.h>
using namespace winrt;
#ifndef THROW_IF_FAILED
#define THROW_IF_FAILED(hr)                                                                                            \
    {                                                                                                                  \
        HRESULT localHresult = (hr);                                                                                   \
        if (FAILED(localHresult))                                                                                      \
            throw hresult_error(localHresult);                                                                         \
    }
#endif

#include <array>
// Note: dxcore.dll needs to be delay-loaded
//#pragma comment(lib, "dxcore.lib")

// For more advanced error-handling, see https://learn.microsoft.com/en-us/cpp/build/reference/error-handling-and-notification?view=msvc-170
static HRESULT CreateDXCore(IDXCoreAdapterFactory** dxCoreFactory)
{
    HRESULT hr = 0;
    __try
    {
        hr = DXCoreCreateAdapterFactory(IID_PPV_ARGS(dxCoreFactory));
    }
    __except (GetExceptionCode() == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)
        ? EXCEPTION_EXECUTE_HANDLER
        : EXCEPTION_CONTINUE_SEARCH)
    {
        // NOTE: We should never see this because XPUInfo::hasDXCore() is called first
        std::cout << "********** DXCore.dll delay-load failure **********\n";
        hr = HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }
    return hr;
}

namespace XI
{
    // DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML is defined in new DX headers or Windows SDK >= 26100
    // See https://github.com/microsoft/DirectX-Headers/releases/tag/v1.614.0
    // Needs to be renamed since it is extern "C"
    DEFINE_GUID(LIBXPUINFO_DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML, 0xb71b0d41, 0x1088, 0x422f, 0xa2, 0x7c, 0x2, 0x50, 0xb7, 0xd3, 0xa9, 0x88);

void XPUInfo::initDXCore()
{
    const bool bPrintInfo = false;

    HRESULT hr = CreateDXCore(m_spFactoryDXCore.put());
    THROW_IF_FAILED(hr);

    const GUID dxGUIDsOlder[] = { DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE };
    const GUID dxGUIDsNewer[] = { LIBXPUINFO_DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML };

    THROW_IF_FAILED(
        m_spFactoryDXCore->CreateAdapterList(ARRAYSIZE(dxGUIDsOlder), dxGUIDsOlder, IID_PPV_ARGS(m_spAdapterList.put())));
    THROW_IF_FAILED(
        m_spFactoryDXCore->CreateAdapterList(ARRAYSIZE(dxGUIDsNewer), dxGUIDsNewer, IID_PPV_ARGS(m_spAdapterList2.put())));

    UI64 luidMinPower = 0ULL, luidHighPerf = 0ULL;

    auto handleSort = [&luidMinPower, &luidHighPerf](const com_ptr<IDXCoreAdapterList>& adapterList)
        {
            bool bSortHighPerformance = adapterList->IsAdapterPreferenceSupported(DXCoreAdapterPreference::HighPerformance);
            bool bSortMinimumPower = adapterList->IsAdapterPreferenceSupported(DXCoreAdapterPreference::MinimumPower);

            if (adapterList->GetAdapterCount() > 0)
            {
                if (bSortMinimumPower)
                {
                    std::array<DXCoreAdapterPreference, 2> prefs = { DXCoreAdapterPreference::Hardware, DXCoreAdapterPreference::MinimumPower };
                    adapterList->Sort((uint32_t)prefs.size(), prefs.data());
                    com_ptr<IDXCoreAdapter> currAdapter = nullptr;
                    THROW_IF_FAILED(adapterList->GetAdapter(0, currAdapter.put()));
                    LUID curLUID{};
                    THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &curLUID));
                    luidMinPower = LuidToUI64(curLUID);
                }

                if (bSortHighPerformance)
                {
                    std::array<DXCoreAdapterPreference, 2> prefs = { DXCoreAdapterPreference::Hardware, DXCoreAdapterPreference::HighPerformance };
                    adapterList->Sort((uint32_t)prefs.size(), prefs.data());
                    com_ptr<IDXCoreAdapter> currAdapter = nullptr;
                    THROW_IF_FAILED(adapterList->GetAdapter(0, currAdapter.put()));
                    LUID curLUID{};
                    THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &curLUID));
                    luidHighPerf = LuidToUI64(curLUID);
                }
            }
        };

    if (bPrintInfo)
        printf("Printing available adapters..\n");

    auto handleAdapter = [this, luidHighPerf, luidMinPower, bPrintInfo](com_ptr<IDXCoreAdapter>& currAdapter)
        {
            // If the adapter is a software adapter then don't consider it for index selection
            bool isHardware, isIntegrated;
            size_t driverDescriptionSize;
            THROW_IF_FAILED(currAdapter->GetPropertySize(DXCoreAdapterProperty::DriverDescription,
                &driverDescriptionSize));
            std::string driverDescription;
            driverDescription.resize(driverDescriptionSize);
            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::IsHardware, &isHardware));

            // This will not succeed for NPUs on some versions of Windows
            HRESULT hr = currAdapter->GetProperty(DXCoreAdapterProperty::IsIntegrated, &isIntegrated);
            bool isIntegratedValid = SUCCEEDED(hr);

            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::DriverDescription,
                driverDescriptionSize, driverDescription.data()));

            // HardwareID
            DXCoreHardwareID hwID;
            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::HardwareID, &hwID));

            LUID curLUID{};
            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &curLUID));

            union
            {
                WORD driverVersion[4];
                UI64 driverVersion64;
            };

            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::DriverVersion,
                sizeof(LARGE_INTEGER), driverVersion));

            bool isGraphics = currAdapter->IsAttributeSupported(DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS);

            uint64_t DedicatedAdapterMemory;
            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::DedicatedAdapterMemory, &DedicatedAdapterMemory));
            uint64_t DedicatedSystemMemory;
            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::DedicatedSystemMemory, &DedicatedSystemMemory));
            uint64_t SharedSystemMemory;
            THROW_IF_FAILED(currAdapter->GetProperty(DXCoreAdapterProperty::SharedSystemMemory, &SharedSystemMemory));

            if (isHardware)
            {
                DXGI_ADAPTER_DESC1 desc1{};
                std::wstring descW = convert(driverDescription);
                lstrcpynW(desc1.Description, descW.c_str(), 128);
                desc1.VendorId = hwID.vendorID;
                desc1.DeviceId = hwID.deviceID;
                desc1.SubSysId = hwID.subSysID;
                desc1.Revision = hwID.revision;
                desc1.DedicatedVideoMemory = DedicatedAdapterMemory;
                desc1.DedicatedSystemMemory = DedicatedSystemMemory;
                desc1.SharedSystemMemory = SharedSystemMemory;
                desc1.AdapterLuid = curLUID;

                if (bPrintInfo)
                {
                    printf("Description: %s (%s), LUID=0x%llx, Version %hu.%hu.%hu.%hu\n", driverDescription.c_str(),
                        isIntegratedValid ? (isIntegrated ? "Integrated" : "Discrete") : "UNKNOWN_UMA",
                        *(uint64_t*)&curLUID,
                        driverVersion[3],
                        driverVersion[2],
                        driverVersion[1],
                        driverVersion[0]
                    );

                    printf("\tDedicated Video: %.3lf, System: %.3lf, Shared: %.3lf\n",
                        DedicatedAdapterMemory / (1024 * 1024 * 1024.0),
                        DedicatedSystemMemory / (1024 * 1024 * 1024.0),
                        SharedSystemMemory / (1024 * 1024 * 1024.0));
                }

                // TODO:  Does the budget and available amount below account for non-DX12 usage?
                if (currAdapter->IsQueryStateSupported(DXCoreAdapterState::AdapterMemoryBudget))
                {
                    DXCoreAdapterMemoryBudgetNodeSegmentGroup nsg{};
                    DXCoreAdapterMemoryBudget memBudget;
                    THROW_IF_FAILED(currAdapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &nsg, &memBudget));

                    if (bPrintInfo && memBudget.budget)
                    {
                        printf("\tLocal Mem:\n");
                        printf("\t\tbudget: %.2lf\n", memBudget.budget / (1024 * 1024 * 1024.0));
                        printf("\t\tcurrentUsage: %.2lf\n", memBudget.currentUsage / (1024 * 1024 * 1024.0));
                        printf("\t\tavailableForReservation: %.2lf\n", memBudget.availableForReservation / (1024 * 1024 * 1024.0));
                        printf("\t\tcurrentReservation: %.2lf\n", memBudget.currentReservation / (1024 * 1024 * 1024.0));
                    }

                    nsg.segmentGroup = DXCoreSegmentGroup::NonLocal;
                    THROW_IF_FAILED(currAdapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &nsg, &memBudget));
                    if (bPrintInfo && memBudget.budget)
                    {
                        printf("\tNonLocal Mem:\n");
                        printf("\t\tbudget: %.2lf\n", memBudget.budget / (1024 * 1024 * 1024.0));
                        printf("\t\tcurrentUsage: %.2lf\n", memBudget.currentUsage / (1024 * 1024 * 1024.0));
                        printf("\t\tavailableForReservation: %.2lf\n", memBudget.availableForReservation / (1024 * 1024 * 1024.0));
                        printf("\t\tcurrentReservation: %.2lf\n", memBudget.currentReservation / (1024 * 1024 * 1024.0));
                    }
                }
                if (bPrintInfo)
                    printf("\n");

                DeviceType devType = isGraphics ? DEVICE_TYPE_GPU : DEVICE_TYPE_NPU;
                DevicePtr newDevice(new Device((UI32)m_Devices.size(), &desc1, devType, API_TYPE_DXCORE, driverVersion64));
                if (!!newDevice)
                {
                    bool isHighPerf = luidHighPerf && (newDevice->getLUID() == luidHighPerf);
                    bool isMinPower = luidMinPower && (newDevice->getLUID() == luidMinPower);

                    auto newIt = m_Devices.find(newDevice->getLUID());
                    if (newIt != m_Devices.end())
                    {
                        // Update
                        // 
                        // Verify driver version
                        if (newDevice->driverVersion().Valid())
                        {
                            if (newDevice->driverVersion().GetAsUI64() !=
                                driverVersion64)
                            {
                                DebugStream dStr(true);
                                dStr << "ERROR: driverVersion mismatch!";
                            }
                        }
                        if (newIt->second->getType() != devType)
                        {
                            DebugStream dStr(true);
                            dStr << "ERROR: DeviceType mismatch!";
                        }

                        newIt->second->initDXCoreDevice(currAdapter, isHighPerf, isMinPower);
                    }
                    else
                    {
                        // Add
                        auto insertResult = m_Devices.insert(std::make_pair(newDevice->getLUID(), newDevice));
                        if (insertResult.second)
                        {
                            insertResult.first->second->initDXCoreDevice(currAdapter, isHighPerf, isMinPower);
                        }
                    }
                    m_UsedAPIs = m_UsedAPIs | API_TYPE_DXCORE;
                } // newDevice valid        
            }
        };

    handleSort(m_spAdapterList);
    for (UINT i = 0; i < m_spAdapterList->GetAdapterCount(); i++)
    {
        com_ptr<IDXCoreAdapter> currAdapter = nullptr;
        THROW_IF_FAILED(m_spAdapterList->GetAdapter(i, currAdapter.put()));
        handleAdapter(currAdapter);
    }

    handleSort(m_spAdapterList2);
    for (UINT i = 0; i < m_spAdapterList2->GetAdapterCount(); i++)
    {
        com_ptr<IDXCoreAdapter> currAdapter = nullptr;
        THROW_IF_FAILED(m_spAdapterList2->GetAdapter(i, currAdapter.put()));
        handleAdapter(currAdapter);
    }

}

void Device::initDXCoreDevice(com_ptr<IDXCoreAdapter>& pDXCoreAdapter, bool bHighPerf, bool bMinPower)
{
    m_pDXCoreAdapter = pDXCoreAdapter;

    // HighPerformance, MinimumPower
    bool isIntegrated;
    bool isDetachable;
    HRESULT hr = pDXCoreAdapter->GetProperty(DXCoreAdapterProperty::IsIntegrated, &isIntegrated);
    bool isIntegratedValid = SUCCEEDED(hr);
    THROW_IF_FAILED(pDXCoreAdapter->GetProperty(DXCoreAdapterProperty::IsDetachable, &isDetachable));

    validAPIs = validAPIs | API_TYPE_DXCORE;
    if (isIntegratedValid)
    {
        updateIfDstVal(m_props.UMA, UMA_UNKNOWN,
            isIntegrated ? UMA_INTEGRATED : NONUMA_DISCRETE);
    }
    updateIfDstNotSet(m_props.IsHighPerformance, I8(bHighPerf));
    updateIfDstNotSet(m_props.IsMinimumPower, I8(bMinPower));
    updateIfDstNotSet(m_props.IsDetachable, I8(isDetachable));
}

DXCoreAdapterMemoryBudget Device::getMemUsage_DXCORE() const
{
    DXCoreAdapterMemoryBudget memUsage{};
    DXCoreAdapterMemoryBudgetNodeSegmentGroup nsg{}; // Want all-zero

    if (XPUInfo::hasDXCore())
    {
        XPUINFO_REQUIRE(m_pDXCoreAdapter);
        THROW_IF_FAILED(m_pDXCoreAdapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &nsg, &memUsage));
    }
    return memUsage;
}

void ScopedRegisterNotification::DXCoreNotificationCallback(DXCoreNotificationType notificationType,
    IUnknown* object,
    void* context)
{
    const ScopedRegisterNotification* pClass = reinterpret_cast<const ScopedRegisterNotification*>(context);
    if (pClass->m_NotificationFunc)
    {
        std::lock_guard<std::mutex> lock(pClass->m_NotificationMutex);
        pClass->m_NotificationFunc(notificationType, object, pClass->m_pXI);
    }
}

void ScopedRegisterNotification::ExampleNotificationFunc_DXCORE(DXCoreNotificationType notificationType, IUnknown* object, const XPUInfo* pXI)
{
    switch (notificationType)
    {
    case DXCoreNotificationType::AdapterListStale:
    {
        std::cout << __FUNCTION__ << ": DXCORE Adapter List Changed" << std::endl;
    }
    break;

    case DXCoreNotificationType::AdapterNoLongerValid:
    {
        winrt::com_ptr<IDXCoreAdapter> pAdapter;
        winrt::check_hresult(object->QueryInterface(pAdapter.put()));

        LUID curLUID{};
        winrt::check_hresult(pAdapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &curLUID));
        auto xiDev = pXI->getDevice(LuidToUI64(curLUID));

        std::cout << __FUNCTION__ << ": DXCORE DEVICE LOST: " << convert(xiDev->name()) << std::endl;
    }
    break;

    case DXCoreNotificationType::AdapterBudgetChange:
    {
        SaveRestoreIOSFlags srFlags(std::cout);
        winrt::com_ptr<IDXCoreAdapter> pAdapter;
        winrt::check_hresult(object->QueryInterface(pAdapter.put()));

        LUID curLUID{};
        winrt::check_hresult(pAdapter->GetProperty(DXCoreAdapterProperty::InstanceLuid, &curLUID));
        auto xiDev = pXI->getDevice(LuidToUI64(curLUID));

        auto newBudget = xiDev->getMemUsage();
        std::cout << __FUNCTION__ << ": Budget changed for device " << convert(xiDev->name()) << 
            " to " << std::setprecision(4) << newBudget.budget / (1024.0 * 1024*1024) << " GB" << std::endl;
    }
    break;

    default:
        XPUINFO_REQUIRE(false);
    }
}

void ScopedRegisterNotification::register_DXCORE(UI64 deviceLUID)
{
    XPUINFO_REQUIRE(m_pXI);
    auto& factory = m_pXI->getDXCoreFactory();
    static_assert((int)DXCoreNotificationType::AdapterBudgetChange < 4 /*m_DXCoreEventCookies.size()*/, "Invalid assumption");
    
    auto pDev = m_pXI->getDevice(deviceLUID);
    XPUINFO_REQUIRE(pDev);
    auto adapter = pDev->getHandle_DXCore();
    // TODO: Also handle m_spAdapterList2
    auto spAdapterList = m_pXI->getDXCoreAdapterList();
    if (factory && adapter && spAdapterList)
    {
        HRESULT hr = factory->RegisterEventNotification(spAdapterList.get(),
            DXCoreNotificationType::AdapterListStale,
            DXCoreNotificationCallback,
            this,
            &m_DXCoreEventCookies[(int)DXCoreNotificationType::AdapterListStale]
        );
        winrt::check_hresult(hr);
        hr = factory->RegisterEventNotification(adapter,
            DXCoreNotificationType::AdapterNoLongerValid,
            DXCoreNotificationCallback,
            this,
            &m_DXCoreEventCookies[(int)DXCoreNotificationType::AdapterNoLongerValid]
        );
        winrt::check_hresult(hr);

        if (m_flags & DXCORE_MEM_BUDGET)
        {
            hr = factory->RegisterEventNotification(adapter,
                DXCoreNotificationType::AdapterBudgetChange,
                DXCoreNotificationCallback,
                this,
                &m_DXCoreEventCookies[(int)DXCoreNotificationType::AdapterBudgetChange]
            );
            winrt::check_hresult(hr);
        }
    }
    if (m_flags & DXCORE_MEM_BUDGET)
    {
        m_bRegisteredAdapterBudgetChange = true;
    }
    m_bRegisteredEvents = true;
}

void ScopedRegisterNotification::unregister_DXCORE()
{
    if (!m_bRegisteredEvents)
    {
        return;
    }

    auto& factory = m_pXI->getDXCoreFactory();
    if (factory && m_bRegisteredEvents)
    {
        winrt::check_hresult(factory->UnregisterEventNotification(m_DXCoreEventCookies[(int)DXCoreNotificationType::AdapterListStale]));
        winrt::check_hresult(factory->UnregisterEventNotification(m_DXCoreEventCookies[(int)DXCoreNotificationType::AdapterNoLongerValid]));
        if (m_bRegisteredAdapterBudgetChange)
        {
            winrt::check_hresult(factory->UnregisterEventNotification(m_DXCoreEventCookies[(int)DXCoreNotificationType::AdapterBudgetChange]));
        }
        if (m_bRegisteredAdapterBudgetChange)
        {
            m_bRegisteredAdapterBudgetChange = false;
        }
        m_bRegisteredEvents = false;
    }
}

} // namespace XI

#endif // #ifdef XPUINFO_USE_DXCORE
