#pragma once
#ifdef _WIN32
#include <wrl.h>
#include <d3d12.h>
#include <list>

namespace XI {

bool CreateD3D12DeviceAndAllocateResource(IUnknown* pAdapter, size_t sizeInBytes, std::list<Microsoft::WRL::ComPtr<ID3D12Resource>>& outResources);

} // XI

#endif // _WIN32
