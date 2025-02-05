// Copyright (C) 2025 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// This header/cpp is intended for use by an application for testing purposes and is not part of LibXPUInfo API.

#pragma once
#ifdef _WIN32
#include <wrl.h>
#include <d3d12.h>
#include <list>

namespace XI {

// For testing purposes, allocate GPU memory in chunks of 16GB or less.
bool CreateD3D12DeviceAndAllocateResource(IUnknown* pAdapter, size_t sizeInBytes, std::list<Microsoft::WRL::ComPtr<ID3D12Resource>>& outResources);

} // XI

#endif // _WIN32
