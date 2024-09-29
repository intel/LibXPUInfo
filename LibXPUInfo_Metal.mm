// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include <Metal/Metal.h>
#include <mach/vm_statistics.h>
#include <sstream>

//#pragma optimize("", off)

namespace
{
static uint64_t util_get_free_memory()
{
    mach_port_t            hostPort;
    mach_msg_type_number_t hostSize;
    vm_size_t              pageSize;
    
    hostPort = mach_host_self();
    hostSize = sizeof(vm_statistics64_data_t) / sizeof(integer_t);
    host_page_size(hostPort, &pageSize);
    
    vm_statistics64_data_t vmStat;
    
    if (host_statistics(hostPort, HOST_VM_INFO, (host_info_t)&vmStat, &hostSize) != KERN_SUCCESS)
    {
        NSLog(@"Failed to fetch vm statistics");
        return UINT64_MAX;
    }
    
    return vmStat.free_count * pageSize;
}

static uint64_t util_get_active_memory()
{
    mach_port_t            hostPort;
    mach_msg_type_number_t hostSize;
    vm_size_t              pageSize;

    hostPort = mach_host_self();
    hostSize = sizeof(vm_statistics64_data_t) / sizeof(integer_t);
    host_page_size(hostPort, &pageSize);

    vm_statistics64_data_t vmStat;

    if (host_statistics(hostPort, HOST_VM_INFO, (host_info_t)&vmStat, &hostSize) != KERN_SUCCESS)
    {
        NSLog(@"Failed to fetch vm statistics");
        return UINT64_MAX;
    }

    //std::cout << "Usage: " << vmStat.active_count * pageSize / (1024*1024*1024ULL) << std::endl;
    return vmStat.active_count * pageSize;
}

static uint64_t util_get_total_memory()
{
    mach_port_t            hostPort;
    mach_msg_type_number_t hostSize;
    vm_size_t              pageSize;

    hostPort = mach_host_self();
    hostSize = sizeof(vm_statistics64_data_t) / sizeof(integer_t);
    host_page_size(hostPort, &pageSize);

    vm_statistics64_data_t vmStat;

    if (host_statistics(hostPort, HOST_VM_INFO, (host_info_t)&vmStat, &hostSize) != KERN_SUCCESS)
    {
        NSLog(@"Failed to fetch vm statistics");
        return UINT64_MAX;
    }

    //std::cout << "Usage: " << vmStat.active_count * pageSize / (1024*1024*1024ULL) << std::endl;
    //return (vmStat.active_count+vmStat.free_count+vmStat.inactive_count+vmStat.wire_count) * pageSize;
    return (vmStat.internal_page_count + vmStat.external_page_count) * pageSize;
}
}
namespace XI
{

DXCoreAdapterMemoryBudget Device::getMemUsage_Metal() const
{
    DXCoreAdapterMemoryBudget mi{};
#if 1
    mi.currentUsage = util_get_active_memory();
    //std::cout << __FUNCTION__ << ": Usage = " << mi.currentUsage / (1024*1024*1024.0) << std::endl;
#elif 0
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (KERN_SUCCESS != task_info(mach_task_self(),
       TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count))
    {
        return mi;
    }
    mi.currentUsage = t_info.resident_size * getpagesize(); // working set
    //*vs  = t_info.virtual_size; // commit
#endif
    
#if 0
    // Not macOS?
    return os_proc_available_memory();
#else
    if (@available(macOS 10.13, *))
    {
        NSArray* metalDevices = MTLCopyAllDevices();
        for (id device in metalDevices)
        {
            uint64_t deviceId       = [device registryID];
            if (deviceId == getLUID())
            {
                // UNUSED auto memUsed = [device currentAllocatedSize];
                auto budget = [device recommendedMaxWorkingSetSize];
                //mi.currentUsage = memUsed;
                mi.budget = budget;
                break;
            }
        }
    }
#endif
    return mi;
}

SystemInfo::SystemInfo()
{
    // The following block won't compile with automatic reference counting
    // NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    // [pool drain];
    // Therefore use the modernized way: @autoreleasepool

    @autoreleasepool {
        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        NSOperatingSystemVersion osVersion = [processInfo operatingSystemVersion];
        OS.BuildNumber = convert(std::to_string(osVersion.majorVersion) + "." + std::to_string(osVersion.minorVersion) + "." + std::to_string(osVersion.patchVersion));
        std::string osVersionStr = [[processInfo operatingSystemVersionString] UTF8String];
        auto buildPos = osVersionStr.find("(Build");
        if (buildPos != osVersionStr.npos)
        {
            OS.BuildNumber += convert(" " + osVersionStr.substr(buildPos));
        }
        OS.Caption = L"macOS " + convert(osVersionStr);
        TotalPhysicalMemory = [processInfo physicalMemory];
        NumberOfProcessors = [processInfo processorCount];
    }
    
    char model[64] = {};
    size_t len=sizeof(model);
    int err = sysctlbyname("hw.model", model, &len, NULL, 0);
    Manufacturer = L"Apple";
    if (!err)
    {
        Model = convert(model);
    }
    
    memset(model, 0, sizeof(model));
    len=sizeof(model);
    err = sysctlbyname("hw.targettype", model, &len, NULL, 0);
    if (!err)
    {
        SystemSKUNumber = convert(model);
    }
    //OS.TotalVirtualMemorySizeKB = util_get_total_memory() / 1024ULL;
    //OS.FreeVirtualMemoryKB = util_get_free_memory() / 1024ULL;
}

UI32 SystemInfo::OSInfo::getUptimeDays() const
{
    NSTimeInterval systemUptime;
    @autoreleasepool {
        systemUptime = [[NSProcessInfo processInfo] systemUptime];
    }
    const double secondsInDay = 60*60*24;
    return (UI32)(systemUptime / secondsInDay);
}

void XPUInfo::initMetal()
{
    m_pSystemInfo.reset(new SystemInfo);
    
    UI64 deviceVersionFromOS = 0;
    @autoreleasepool {
        NSProcessInfo *processInfo = [NSProcessInfo processInfo];
        NSOperatingSystemVersion osVersion = [processInfo operatingSystemVersion];
        deviceVersionFromOS = (UI64(osVersion.majorVersion) << 16*3) |
            (UI64(osVersion.minorVersion) << 16*2) |
            (UI64(osVersion.patchVersion) << 16*1);
    }
    
    int adapterIndex=0;
    if (@available(macOS 10.13, *))
    {
        NSArray* metalDevices = MTLCopyAllDevices();
        for (id device in metalDevices)
        {
            DXGI_ADAPTER_DESC1 desc{};
                        
            const char* cdeviceName = [[device name] UTF8String];
            uint64_t deviceId       = [device registryID];
            BOOL isRemovable        = [device isRemovable];
            BOOL isHighPerformance  = ![device isLowPower]; // The property is typically true for integrated GPUs and false for discrete GPUs. However, an Apple silicon GPU on a Mac sets the property to false because it doesn’t need to lower its performance to conserve energy.
            
            WString name(convert(cdeviceName));
            wcsncpy(desc.Description, name.c_str(), 128);
            desc.AdapterLuid.ui64 = deviceId;
            desc.SharedSystemMemory = [device recommendedMaxWorkingSetSize];
            
            DevicePtr newDevice(new Device(adapterIndex++, &desc, DEVICE_TYPE_GPU, API_TYPE_METAL, deviceVersionFromOS));
            I64 bw = [device maxTransferRate];
            newDevice->m_props.MemoryBandWidthMax = (bw > 0) ? bw : -1LL;
            newDevice->m_props.UMA = [device hasUnifiedMemory] ? UMA_INTEGRATED : NONUMA_DISCRETE;
            newDevice->validAPIs = API_TYPE_METAL;
            newDevice->m_props.IsDetachable = isRemovable;
            newDevice->m_props.IsHighPerformance = isHighPerformance;
            
            m_Devices.insert(std::make_pair(deviceId, newDevice));
            if (!(m_UsedAPIs & API_TYPE_METAL))
            {
                m_UsedAPIs = m_UsedAPIs | API_TYPE_METAL;
            }
        }
    }
}

#if 0
// See https://stackoverflow.com/questions/10110658/programmatically-get-gpu-percent-usage-in-os-x
#include <CoreFoundation/CoreFoundation.h>
#include <Cocoa/Cocoa.h>
#include <IOKit/IOKitLib.h>

int main(int argc, const char * argv[])
{

while (1) {

    // Get dictionary of all the PCI Devicces
    CFMutableDictionaryRef matchDict = IOServiceMatching(kIOAcceleratorClassName);

    // Create an iterator
    io_iterator_t iterator;

    if (IOServiceGetMatchingServices(kIOMasterPortDefault,matchDict,
                                     &iterator) == kIOReturnSuccess)
    {
        // Iterator for devices found
        io_registry_entry_t regEntry;

        while ((regEntry = IOIteratorNext(iterator))) {
            // Put this services object into a dictionary object.
            CFMutableDictionaryRef serviceDictionary;
            if (IORegistryEntryCreateCFProperties(regEntry,
                                                  &serviceDictionary,
                                                  kCFAllocatorDefault,
                                                  kNilOptions) != kIOReturnSuccess)
            {
                // Service dictionary creation failed.
                IOObjectRelease(regEntry);
                continue;
            }

            CFMutableDictionaryRef perf_properties = (CFMutableDictionaryRef) CFDictionaryGetValue( serviceDictionary, CFSTR("PerformanceStatistics") );
            if (perf_properties) {

                static ssize_t gpuCoreUse=0;
                static ssize_t freeVramCount=0;
                static ssize_t usedVramCount=0;

                const void* gpuCoreUtilization = CFDictionaryGetValue(perf_properties, CFSTR("GPU Core Utilization"));
                const void* freeVram = CFDictionaryGetValue(perf_properties, CFSTR("vramFreeBytes"));
                const void* usedVram = CFDictionaryGetValue(perf_properties, CFSTR("vramUsedBytes"));
                if (gpuCoreUtilization && freeVram && usedVram)
                {
                    CFNumberGetValue( (CFNumberRef) gpuCoreUtilization, kCFNumberSInt64Type, &gpuCoreUse);
                    CFNumberGetValue( (CFNumberRef) freeVram, kCFNumberSInt64Type, &freeVramCount);
                    CFNumberGetValue( (CFNumberRef) usedVram, kCFNumberSInt64Type, &usedVramCount);
                    NSLog(@"GPU: %.3f%% VRAM: %.3f%%",gpuCoreUse/(double)10000000,usedVramCount/(double)(freeVramCount+usedVramCount)*100.0);

                }

            }

            CFRelease(serviceDictionary);
            IOObjectRelease(regEntry);
        }
        IOObjectRelease(iterator);
    }

   sleep(1);
}
return 0;
}
#endif
} // XI
