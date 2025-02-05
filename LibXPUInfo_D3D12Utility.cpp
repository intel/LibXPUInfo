#ifdef _WIN32

#include "LibXPUInfo.h"
#include "LibXPUInfo_D3D12Utility.h"
#include <stdexcept>
#include <iostream>
#include <dxcore.h>

using Microsoft::WRL::ComPtr;

namespace XI {

// NOTE: This can only be called once we know that DXCore is available
bool CreateD3D12DeviceAndAllocateResource(IUnknown* pAdapter, size_t sizeInBytes, std::list<Microsoft::WRL::ComPtr<ID3D12Resource>>& outResources)
{
    try
    {
        // Enable the D3D12 debug layer.
#if 0 //defined(_DEBUG)
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
        }
#endif
        ComPtr<IDXCoreAdapter> dxcoreAdapter;
        XPUINFO_REQUIRE_MSG(SUCCEEDED(pAdapter->QueryInterface(dxcoreAdapter.GetAddressOf())), "Error getting DXCore Adapter");

        bool memRequestOk = false;
        // Requesting >16GB on Intel Meteor Lake with 32GB system RAM results in stack corruption exception in driver (unrecoverable).
        // Need to test on other devices, but for now set limit of min(16GB, memBudget).
        constexpr size_t maxIntelIntegratedAllocSize = 16ULL * 1024 * 1024 * 1024;
        if (dxcoreAdapter->IsQueryStateSupported(DXCoreAdapterState::AdapterMemoryBudget))
        {
            DXCoreAdapterMemoryBudgetNodeSegmentGroup nsg{};
            DXCoreAdapterMemoryBudget memBudget;
            /*THROW_IF_FAILED*/
            HRESULT hr = dxcoreAdapter->QueryState(DXCoreAdapterState::AdapterMemoryBudget, &nsg, &memBudget);
            if (FAILED(hr))
            {
                std::cout << __FUNCTION__ << ": Error getting adapter memory budget\n";
            }
            else if (memBudget.budget < sizeInBytes)
            {
                std::cout << __FUNCTION__ << ": Memory requested, " << sizeInBytes / (1024.0 * 1024 * 1024) <<
                    " GB, exceeds available, " << memBudget.budget / (1024.0 * 1024 * 1024) << " GB\n";
            }
            else
            {
                memRequestOk = true;
            }
        }

        if (memRequestOk)
        {
            // Create the D3D12 device.
            ComPtr<ID3D12Device> device;
            if (FAILED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device))))
            {
                throw std::runtime_error("Failed to create D3D12 device.");
            }

            auto lastResourceSize = sizeInBytes % maxIntelIntegratedAllocSize;
            auto resourceSize = sizeInBytes / maxIntelIntegratedAllocSize;
            auto numResourcesToAllocate = resourceSize + (lastResourceSize!=0);

            // Describe and create a committed resource.
            D3D12_HEAP_PROPERTIES heapProperties = {};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProperties.CreationNodeMask = 1;
            heapProperties.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = sizeInBytes;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.SampleDesc.Quality = 0;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            for (size_t i = 0; i < numResourcesToAllocate; ++i)
            {
                ComPtr<ID3D12Resource> resource;
                resourceDesc.Width = (i==numResourcesToAllocate-1) ? lastResourceSize : maxIntelIntegratedAllocSize;

                if (FAILED(device->CreateCommittedResource(
                    &heapProperties,
                    D3D12_HEAP_FLAG_NONE,
                    &resourceDesc,
                    D3D12_RESOURCE_STATE_COMMON,
                    nullptr,
                    IID_PPV_ARGS(&resource))))
                {
                    throw std::runtime_error("Failed to create committed resource.");
                }
                outResources.emplace_back(std::move(resource));
            }
        }
    }
    catch (const std::runtime_error& e)
    {
        std::cout << "CreateCommittedResource() failed: " << e.what() << std::endl;
        return false;
    }
    catch (...)
    {
        //throw std::runtime_error("Unhandled exception calling CreateCommittedResource()");
        std::cout << "Unhandled exception calling CreateCommittedResource()\n";
        return false;
    }
    return true;
}

} // XI

#endif // _WIN32
