// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Some parts of this are actually cross-platform, despite "WMI" name
#if defined(XPUINFO_USE_WMI) || defined(__APPLE__)
#define _WIN32_DCOM
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <iostream>
#ifdef _WIN32
#include <comdef.h>
#include <Wbemidl.h>
#endif

#include <iostream>
#include <sstream>
#include <iomanip>
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#ifdef _WIN32
#pragma comment(lib, "wbemuuid.lib")
#endif

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

namespace XI
{
#ifdef _WIN32
namespace // private
{
template <typename outT, typename strT>
bool parseFromString(outT& out, const strT& inSring)
{
    std::wistringstream strm(inSring);
    outT outVal;
    strm >> outVal;
    if (!strm.bad())
    {
        out = outVal;
        return true;
    }
    return false;
}

std::wstring getVariantDate(const wchar_t* name, IWbemClassObject* pclsObj)
{
    std::wstring outDate;
    VARIANT vtProp;
    HRESULT hres = pclsObj->Get(name, 0, &vtProp, 0, 0);
    if (SUCCEEDED(hres) && vtProp.bstrVal)
    {
        // See https://learn.microsoft.com/en-us/windows/win32/wmisdk/cim-datetime
        // yyyymmddHHMMSS.mmmmmmsUUU
        // 20240221000000.000000-000
        std::wstring date(vtProp.bstrVal);
        std::wstring year(date.substr(0, 4));
        std::wstring month(date.substr(4, 2));
        std::wstring day(date.substr(6, 2));

        outDate = year + L"-" + month + L"-" + day;
    }
    VariantClear(&vtProp);
    return outDate;
}

std::wstring getVariantWStr(const wchar_t* name, IWbemClassObject* pclsObj)
{
    VARIANT vtProp;
    std::wstring outStr;
    HRESULT hres = pclsObj->Get(name, 0, &vtProp, 0, 0);
    if (SUCCEEDED(hres) && vtProp.bstrVal)
    {
        outStr = vtProp.bstrVal;
    }
    VariantClear(&vtProp);
    return outStr;
}

#if 0 // Need to use getVariantUI64FromStr instead!
XI::UI64 getVariantUI64(const wchar_t* name, IWbemClassObject* pclsObj)
{
    VARIANT vtProp;
    XI::UI64 outVal = -1;
    HRESULT hres = pclsObj->Get(name, 0, &vtProp, 0, 0);

    if (SUCCEEDED(hres))
    {
        outVal = vtProp.ullVal;
    }
    VariantClear(&vtProp);
    return outVal;
}
#endif

UI32 getVariantUI32(const wchar_t* name, IWbemClassObject* pclsObj)
{
    VARIANT vtProp;
    UI32 outVal = UI32(-1);
    HRESULT hres = pclsObj->Get(name, 0, &vtProp, 0, 0);

    if (SUCCEEDED(hres))
    {
        outVal = vtProp.uintVal;
    }
    VariantClear(&vtProp);
    return outVal;
}

UI16 getVariantUI16(const wchar_t* name, IWbemClassObject* pclsObj)
{
    VARIANT vtProp;
    UI16 outVal = UI16(-1);
    HRESULT hres = pclsObj->Get(name, 0, &vtProp, 0, 0);

    if (SUCCEEDED(hres))
    {
        outVal = vtProp.uiVal;
    }
    VariantClear(&vtProp);
    return outVal;
}

UI64 getVariantUI64FromStr(const wchar_t* name, IWbemClassObject* pclsObj) noexcept
{
    UI64 outVal = UI64(-1);
    try
    {
        VARIANT vtProp;
        HRESULT hres = pclsObj->Get(name, 0, &vtProp, 0, 0);

        if (SUCCEEDED(hres) && vtProp.bstrVal)
        {
            XI::UI64 tval = 0;
            if (parseFromString(tval, vtProp.bstrVal))
            {
                outVal = tval;
            }
        }
        VariantClear(&vtProp);
    }
    // Don't allow parse exception to bubble up, already marked as failure (-1)
    catch (...)
    {
    }

    return outVal;
}

// Based on sample at https://learn.microsoft.com/en-us/windows/win32/wmisdk/example-creating-a-wmi-application?redirectedfrom=MSDN
class WMIReader
{
public:
    WMIReader()
    {
        m_HR = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (!FAILED(m_HR))
        {
            m_CoInit = true;
            // Initialize - default security implicitly initialized

            if (!FAILED(m_HR))
            {
                m_HR = CoCreateInstance(
                    CLSID_WbemLocator,
                    0,
                    CLSCTX_INPROC_SERVER,
                    IID_IWbemLocator, (LPVOID*)&m_pLoc);

                if (!FAILED(m_HR) && m_pLoc)
                {
                    // Connect to the root\cimv2 namespace with the
                    // current user and obtain pointer pSvc
                    // to make IWbemServices calls.

                    m_HR = m_pLoc->ConnectServer(

                        _bstr_t(L"ROOT\\CIMV2"), // WMI namespace
                        nullptr,                    // User name
                        nullptr,                    // User password
                        0,                       // Locale
                        0,                       // Security flags
                        0,                       // Authority
                        0,                       // Context object
                        &m_pSvc                  // IWbemServices proxy
                    );

                    if (!FAILED(m_HR) && m_pSvc)
                    {
                        // Set the IWbemServices proxy so that impersonation
                        // of the user (client) occurs.
                        m_HR = CoSetProxyBlanket(

                            m_pSvc,                       // the proxy to set
                            RPC_C_AUTHN_WINNT,            // authentication service
                            RPC_C_AUTHZ_NONE,             // authorization service
                            nullptr,                         // Server principal name
                            RPC_C_AUTHN_LEVEL_CALL,       // authentication level
                            RPC_C_IMP_LEVEL_IMPERSONATE,  // impersonation level
                            nullptr,                         // client identity
                            EOAC_NONE                     // proxy capabilities
                        );
                    }
                }
            }
        }
        XPUINFO_REQUIRE(!FAILED(m_HR) && m_pSvc);
    };
    IEnumWbemClassObject* getEnumerator(const wchar_t* name) const
    {
        IEnumWbemClassObject* pEnumerator = nullptr;
        XPUINFO_REQUIRE(m_pSvc);
        HRESULT hres = m_pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t((std::wstring(L"SELECT * FROM ") + name).c_str()),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &pEnumerator);
        if (SUCCEEDED(hres))
        {
            return pEnumerator;
        }
        return nullptr;
    }

    class WMIClassObject : protected NoCopyAssign
    {
    public:
        friend class WMIReader;
        bool valid() const { return !!m_pEnumerator; }
        bool advance()
        {
            if (m_pEnumerator)
            {
                ULONG uReturn = 0;
                HRESULT hres = m_pEnumerator->Next(WBEM_INFINITE, 1,
                    &m_pclsObj, &uReturn);
                if (SUCCEEDED(hres))
                {
                    if (0 == uReturn)
                    {
                        return false;
                    }
                    return true;
                }
            }
            return false;
        }
        ~WMIClassObject()
        {
            if (m_pEnumerator)
            {
                m_pEnumerator->Release();
            }
        }
        IWbemClassObject* get() { return m_pclsObj; }

    protected:
        WMIClassObject(const WMIReader& reader, const wchar_t* name) : m_Reader(reader)
        {
            m_pEnumerator = m_Reader.getEnumerator(name);
        }
        const WMIReader& m_Reader;
        IEnumWbemClassObject* m_pEnumerator;
        IWbemClassObject* m_pclsObj = nullptr;
    };
    WMIClassObject getClassObj(const wchar_t* name)
    {
        return WMIClassObject(*this, name);
    }
    ~WMIReader()
    {
        if (m_pSvc)
        {
            m_pSvc->Release();
        }
        if (m_pLoc)
        {
            m_pLoc->Release();
        }
        if (m_CoInit)
        {
            CoUninitialize();
        }
    }

protected:
    HRESULT m_HR;
    bool m_CoInit = false;
    // Obtain the initial locator to Windows Management
    // on a particular host computer.
    IWbemLocator* m_pLoc = nullptr;
    IWbemServices* m_pSvc = nullptr;
};

} // namespace private

SystemInfo::SystemInfo()
{
#define XPUINFO_WMI_INIT_TIMER 0
#if XPUINFO_WMI_INIT_TIMER
    Timer timer;
    timer.Start();
#endif
    {
        WMIReader wmi;
        typedef std::unique_ptr<std::thread> ThreadPtr;
        struct TaskThread
        {
            TaskThread(ThreadPtr tp) : m_tp(std::move(tp)) {}
            ThreadPtr m_tp;
        };
        std::vector<TaskThread> threads;

#if 1
#define RUNTASK(t) threads.emplace_back(TaskThread(ThreadPtr(new std::thread(t))));
#else
#define RUNTASK(t) t()
#endif

        if (1)
        {
            auto getOS = [&]()
            {
                auto classObj_OperatingSystem = wmi.getClassObj(L"Win32_OperatingSystem");
                while (classObj_OperatingSystem.valid())
                {
                    if (!classObj_OperatingSystem.advance())
                    {
                        break;
                    }
                    auto pclsObj = classObj_OperatingSystem.get();
                    OS.BuildNumber = getVariantWStr(L"BuildNumber", pclsObj);
                    //WString BuildType = getVariantWStr(L"BuildType", pclsObj); // rel/dbg
                    OS.Caption = getVariantWStr(L"Caption", pclsObj);

                    OS.CodeSet = getVariantWStr(L"CodeSet", pclsObj);
                    OS.CountryCode = getVariantWStr(L"CountryCode", pclsObj);
                    //WString Manufacturer = getVariantWStr(L"Manufacturer", pclsObj);
                    //WString Description = getVariantWStr(L"Description", pclsObj);
                    OS.FreePhysicalMemoryKB = getVariantUI64FromStr(L"FreePhysicalMemory", pclsObj);
                    OS.FreeSpaceInPagingFilesKB = getVariantUI64FromStr(L"FreeSpaceInPagingFiles", pclsObj);
                    OS.FreeVirtualMemoryKB = getVariantUI64FromStr(L"FreeVirtualMemory", pclsObj);
                    //UI64 TotalSwapSpaceSizeKB = getVariantUI64FromStr(L"TotalSwapSpaceSize", pclsObj);
                    OS.TotalVirtualMemorySizeKB = getVariantUI64FromStr(L"TotalVirtualMemorySize", pclsObj);
                    OS.TotalVisibleMemorySizeKB = getVariantUI64FromStr(L"TotalVisibleMemorySize", pclsObj);
                    OS.Name = getVariantWStr(L"Name", pclsObj);
                    OS.LastBootUpDate = getVariantDate(L"LastBootUpTime", pclsObj);
                    OS.LocalDate = getVariantDate(L"LocalDateTime", pclsObj);
                    OS.Locale = getVariantWStr(L"Locale", pclsObj);
                    //UI32 OperatingSystemSKU = getVariantUI32(L"OperatingSystemSKU", pclsObj);
                    OS.OSArchitecture = getVariantWStr(L"OSArchitecture", pclsObj);
                    //UI32 OSLanguage = getVariantUI32(L"OSLanguage", pclsObj);
                    //WString OtherTypeDescription = getVariantWStr(L"OtherTypeDescription", pclsObj);
                    //UI16 ServicePackMajorVersion = getVariantUI16(L"ServicePackMajorVersion", pclsObj);
                    //UI16 ServicePackMinorVersion = getVariantUI16(L"ServicePackMinorVersion", pclsObj);
                    //WString Version = getVariantWStr(L"Version", pclsObj); // i.e. 10.0.22621
                    //std::wcout << Version << ": " << ServicePackMajorVersion << ":" << ServicePackMinorVersion << std::endl;
                    /*
                    * TODO: May need to go to registry if we need full version number
                    * i.e. Computer\HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Orchestrator\OSUpgrade,
                    * and/or HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\DisplayVersion
                    * Or see https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-quickfixengineering for details on updates
                    */
                }
            };
            RUNTASK(getOS);
        }

        if (1)
        {
            auto getCS = [&]()
            {
                auto classObj_ComputerSystem = wmi.getClassObj(L"Win32_ComputerSystem");
                while (classObj_ComputerSystem.valid())
                {
                    if (!classObj_ComputerSystem.advance())
                    {
                        break;
                    }
                    auto pclsObj = classObj_ComputerSystem.get();
                    Manufacturer = getVariantWStr(L"Manufacturer", pclsObj);
                    Model = getVariantWStr(L"Model", pclsObj);
                    NumberOfLogicalProcessors = getVariantUI32(L"NumberOfLogicalProcessors", pclsObj);
                    NumberOfProcessors = getVariantUI32(L"NumberOfProcessors", pclsObj);
                    SystemFamily = getVariantWStr(L"SystemFamily", pclsObj);
                    SystemSKUNumber = getVariantWStr(L"SystemSKUNumber", pclsObj);
                    SystemType = getVariantWStr(L"SystemType", pclsObj);
                    TotalPhysicalMemory = getVariantUI64FromStr(L"TotalPhysicalMemory", pclsObj);
                }
            };
            RUNTASK(getCS);
        }

        if (1)
        {
            auto getProc = [&]()
            {
                auto classObj_Processor = wmi.getClassObj(L"Win32_Processor");
                while (classObj_Processor.valid())
                {
                    if (!classObj_Processor.advance())
                    {
                        break;
                    }
                    auto pclsObj = classObj_Processor.get();
                    Processor proc;
                    proc.ClockSpeedMHz = getVariantUI32(L"MaxClockSpeed", pclsObj);
                    proc.NumberOfCores = getVariantUI32(L"NumberOfCores", pclsObj);
                    proc.NumberOfEnabledCores = getVariantUI32(L"NumberOfEnabledCore", pclsObj);
                    proc.NumberOfLogicalProcessors = getVariantUI32(L"NumberOfLogicalProcessors", pclsObj);

                    Processors.push_back(proc);
                }
            };
            RUNTASK(getProc);
        }
        if (1)
        {
            auto getBIOS = [&]()
            {
                auto classObj_BIOS = wmi.getClassObj(L"Win32_BIOS");
                while (classObj_BIOS.valid())
                {
                    if (!classObj_BIOS.advance())
                    {
                        break;
                    }
                    auto pclsObj = classObj_BIOS.get();
                    BIOS.Name = getVariantWStr(L"Name", pclsObj);
                    BIOS.Manufacturer = getVariantWStr(L"Manufacturer", pclsObj);
                    BIOS.SerialNumber = getVariantWStr(L"SerialNumber", pclsObj);
                    BIOS.Version = getVariantWStr(L"Version", pclsObj);
                    BIOS.ReleaseDate = getVariantDate(L"ReleaseDate", pclsObj);
                }
            };
            RUNTASK(getBIOS);
        }
        if (1)
        {
            auto getVideo = [&]()
            {
                auto classObj_VideoController = wmi.getClassObj(L"Win32_VideoController");
                while (classObj_VideoController.valid())
                {
                    if (!classObj_VideoController.advance())
                    {
                        break;
                    }
                    auto pclsObj = classObj_VideoController.get();
                    VideoControllers.emplace_back(
                        VideoControllerInfo
                        {
                            getVariantWStr(L"Name", pclsObj),
                            getVariantWStr(L"VideoModeDescription", pclsObj),
                            getVariantUI32(L"CurrentRefreshRate", pclsObj),
                            getVariantWStr(L"InfSection", pclsObj),
                            getVariantWStr(L"PNPDeviceID", pclsObj)
                        }
                    );
                }
            };
            RUNTASK(getVideo);
        }

        if (1)
        {
            auto getMemory = [&]()
            {
                auto classObj_Memory = wmi.getClassObj(L"Win32_PhysicalMemory");
                while (classObj_Memory.valid())
                {
                    if (!classObj_Memory.advance())
                    {
                        break;
                    }
                    auto pclsObj = classObj_Memory.get();

                    MemoryDeviceInfo mem{
                        getVariantUI32(L"ConfiguredClockSpeed", pclsObj),
                        getVariantUI64FromStr(L"Capacity", pclsObj)
                    };
                    auto it = m_mapMemSize.find(mem);
                    if (it == m_mapMemSize.end())
                    {
                        m_mapMemSize.insert(std::make_pair(mem, 1));
                    }
                    else
                    {
                        ++it->second;
                    }
                }
            };
            RUNTASK(getMemory);
        }

        for (auto& t : threads)
        {
            if (t.m_tp.get())
            {
                t.m_tp->join();
            }
        }
    }
#if XPUINFO_WMI_INIT_TIMER
    timer.Stop();
    std::cout << __FUNCTION__ << ": " << timer.GetElapsedSecs() << " seconds\n";
#endif
}

void XPUInfo::initWMI()
{
    try
    {
        m_pSystemInfo.reset(new SystemInfo);
        m_UsedAPIs = m_UsedAPIs | API_TYPE_WMI;
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception caught in " << __FUNCTION__ << ":" << e.what() << std::endl;
    }
    catch (...)
    {
    }

    if (!(m_UsedAPIs & API_TYPE_WMI))
    {
        std::cout << "WMI Init Failed!\n";
    }

}
#endif // _WIN32 only

// For Mac: system_profiler -json SPMemoryDataType SPHardwareDataType
WString SystemInfo::getMemoryDescription() const
{
    std::wostringstream os;
#ifdef _WIN32
    double totalMem = 0.;
    int i = 0;
    for (auto [dev, count] : m_mapMemSize)
    {
        totalMem += count * dev.Capacity;
        if (i > 0)
        {
            os << ", ";
        }
        os << count << L" x " << dev.Capacity / (1024.0 * 1024 * 1024) << "GB at " << dev.SpeedMHz << "MHz";
        ++i;
    }
    os << " (" << totalMem / (1024.0 * 1024 * 1024) << "GB Total)";
#endif
    return os.str();
}

UI32 SystemInfo::getMemorySpeed() const
{
    if (m_mapMemSize.size())
    {
        // Assuming systems do not actually mix speed
        return m_mapMemSize.begin()->first.SpeedMHz;
    }
    return 0;
}

UI32 SystemInfo::getMemoryDeviceCount() const
{
    UI32 devCount = 0;
    for (auto [dev, count] : m_mapMemSize)
    {
        devCount += count;
    }
    return devCount;
}

// See LibXPUInfo_Metal.mm for Apple impl
#if !defined(__APPLE__)
UI32 SystemInfo::OSInfo::getUptimeDays() const
{
    // Not very accurate, but that's ok
#ifdef _WIN32
    UI32 days0 = 0, days1 = 0;
    std::wistringstream date0(LastBootUpDate);
    std::wistringstream date1(LocalDate);
    {
        UI32 year, month, day;
        wchar_t c;
        date0 >> year >> c >> month >> c >> day >> c;
        days0 = year * 365 + month * 30 + day;
    }
    {
        UI32 year, month, day;
        wchar_t c;
        date1 >> year >> c >> month >> c >> day >> c;
        days1 = year * 365 + month * 30 + day;
    }

    return days1 - days0;
#elif defined(__linux__)
    struct sysinfo si{};
    int error = sysinfo(&si);
    if (!error)
    {
        const double secondsInDay = 60 * 60 * 24;
        return (UI32)(si.uptime / secondsInDay);
    }
    return 0;
#endif
}
#endif // !APPLE

std::ostream& operator<<(std::ostream& os, const SystemInfo& si)
{
    SaveRestoreIOSFlags srFlags(os);
    using namespace std;
    const int colW = 28+8;
    os << left << setw(colW) << "\tManufacturer:" << convert(si.Manufacturer) << endl;
    if (si.Model != L"System Product Name")
    {
        os << left << setw(colW) << "\tModel:" << convert(si.Model) << endl;
    }
    if (si.SystemFamily.length() && (si.SystemFamily != L"To be filled by O.E.M."))
    {
        os << left << setw(colW) << "\tSystemFamily:" << convert(si.SystemFamily) << endl;
    }
    if (si.SystemSKUNumber.length() && (si.SystemSKUNumber != L"SKU"))
    {
        os << left << setw(colW) << "\tSystemSKUNumber:" << convert(si.SystemSKUNumber) << endl;
    }
    if (si.SystemType.length() && (si.SystemType != L"x64-based PC"))
    {
        os << left << setw(colW) << "\tSystemType:" << convert(si.SystemType) << endl;
    }
    os << left << setw(colW) << "\tTotalPhysicalMemory (GB):" << setprecision(4) << si.TotalPhysicalMemory / (1024.0 * 1024 * 1024) << endl;
    if (si.OS.TotalVirtualMemorySizeKB)
    {
        os << left << setw(colW) << "\tTotalVirtualMemory (GB):" << setprecision(4) << si.OS.TotalVirtualMemorySizeKB / (1024.0 * 1024) << endl;
    }
    if (si.OS.FreePhysicalMemoryKB)
    {
        os << left << setw(colW) << "\tFreePhysicalMemory (GB):" << setprecision(4) << si.OS.FreePhysicalMemoryKB / (1024.0 * 1024) << endl;
    }
    if (si.OS.FreeVirtualMemoryKB)
    {
        os << left << setw(colW) << "\tFreeVirtualMemory (GB):" << setprecision(4) << si.OS.FreeVirtualMemoryKB / (1024.0 * 1024) << endl;
    }
    if (si.m_mapMemSize.size())
    {
        os << std::left << std::setw(colW) << "\tPhysical Memory:" << convert(si.getMemoryDescription()) << endl;
    }
    os << std::left << std::setw(colW) << "\tOS:" << convert(si.OS.Caption);
    if (si.OS.BuildNumber.length())
    {
        os << ", Build " << convert(si.OS.BuildNumber);
    }
    os << endl;
    os << std::left << std::setw(colW) << "\tUptime (Days):" << si.OS.getUptimeDays() << endl;

    if (si.BIOS.SerialNumber.length() && (si.BIOS.SerialNumber != L"System Serial Number"))
    {
        os << std::left << std::setw(colW) << "\tSerialNumber:" << convert(si.BIOS.SerialNumber) << endl;
    }
    if (si.BIOS.Name.length())
    {
        os << std::left << std::setw(colW) << "\tBIOS Name:" << convert(si.BIOS.Name) << endl;
    }
    if (si.BIOS.Manufacturer.length())
    {
        os << std::left << std::setw(colW) << "\tBIOS Manufacturer:" << convert(si.BIOS.Manufacturer) << endl;
    }
    if (si.BIOS.Version.length())
    {
        os << std::left << std::setw(colW) << "\tBIOS Version:" << convert(si.BIOS.Version) << endl;
    }
    if (si.BIOS.ReleaseDate.length())
    {
        os << std::left << std::setw(colW) << "\tBIOS ReleaseDate:" << convert(si.BIOS.ReleaseDate) << endl;
    }
    if (si.VideoControllers.size())
    {
        // TODO: Correlate with Device and add to that section instead
        os << "\nDisplays:\n";
        for (const auto& vc : si.VideoControllers)
        {
            if (!vc.VideoMode.empty())
            {
                std::ostringstream col0;
                col0 << "\t" << convert(vc.Name) << ": ";
                os << std::left << std::setw(colW) << col0.str() << convert(vc.VideoMode) << " @ " << vc.RefreshRate << "Hz\n";
            }
        }
    }
    return os;
}

} // namespace XI

#endif // XPUINFO_USE_WMI

