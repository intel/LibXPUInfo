// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Utility code for using XPUInfo out-of-process
#ifdef XPUINFO_USE_IPC
#include "LibXPUInfo_IPC.h"
#define OPEN_FILE_MAPPING_ERROR     ((DWORD)0xC00007D0L)
#define UNABLE_MAP_VIEW_OF_FILE     ((DWORD)0xC00007D1L)

namespace XI
{
#ifdef _WIN32
namespace Win
{

ProcessInformation::~ProcessInformation()
{
    if (hProcess)
    {
        CloseHandle(hProcess);
    }
    if (hThread)
    {
        CloseHandle(hThread);
    }
}

NamedEvent::NamedEvent(const char* sharedName)
{
    m_hEvent = CreateEventA(nullptr, FALSE, FALSE, sharedName);
    XPUINFO_REQUIRE(m_hEvent);
}

NamedMutex::NamedMutex(const char* sharedName)
{
    m_hMutex = CreateMutexA(nullptr, FALSE, sharedName);
    m_CreateError = GetLastError();
    XPUINFO_REQUIRE(m_hMutex);
}

NamedMutex::ScopedLock::ScopedLock(NamedMutex& m, UI32 timeout, UI32* pResult)
    :
    m_Mutex(m), m_pOutResult(pResult)
{
    m_Result = WaitForSingleObject(m.m_hMutex, timeout);
    if (m_pOutResult)
    {
        *m_pOutResult = m_Result;
    }
}

NamedSemaphore::NamedSemaphore(const char* name, const I32 initialCount)
{
    m_hSem = CreateSemaphoreA(nullptr, initialCount, initialCount, name);
    m_CreateError = GetLastError();
}

NamedSemaphore::ScopedAcquire::ScopedAcquire(NamedSemaphore& sem, UI32 timeout) :
    m_sem(sem)
{
    m_Result = WaitForSingleObject(sem.m_hSem, timeout);
}

NamedSemaphore::ScopedAcquire::~ScopedAcquire()
{
    if (m_Result == WAIT_OBJECT_0)
    {
        UI32 res = ReleaseSemaphore(m_sem.m_hSem, 1, nullptr);
        XPUINFO_REQUIRE(res);
    }
}

NamedSharedMemory::NamedSharedMemory(size_t size, const char* sharedName, bool bReadOnlyAccess) : 
    m_Size(size),
    m_hMutex((std::string(sharedName) + "_MUTEX").c_str())
{
    static_assert(sizeof(size_t) == 8, "Assuming 64-bit");
    m_hSharedMemory = CreateFileMappingA(
        (HANDLE)INVALID_HANDLE_VALUE,
        nullptr,                   // no security
        PAGE_READWRITE,         // to allow read & write access
        (DWORD)(size >> 32),
        (DWORD)size,            // file size
        sharedName);            // object name

    DWORD MemStatus = GetLastError();    // to see if this is the first opening
    if (m_hSharedMemory == nullptr)
    {
        m_Status = OPEN_FILE_MAPPING_ERROR;
        // this is fatal, if we can't get data then there's no
        // point in continuing.
    }
    else
    {
        NamedMutex::ScopedLock lock(m_hMutex);
        if (MemStatus != ERROR_ALREADY_EXISTS)
        {
            // this is the first access to the file so initialize the
            // instance count
            m_pMappedMemory = MapViewOfFile(
                m_hSharedMemory,  // shared mem handle
                FILE_MAP_WRITE,         // access desired
                0,                      // starting offset
                0,
                0);                     // map the entire object
            if (m_pMappedMemory != nullptr)
            {
                // if here, then pdwInstanceCount should be valid
                // so initialize the shared memory structure
                // clear memory block
                memset(m_pMappedMemory, 0, size);

                m_Status = ERROR_SUCCESS;
            }
            else
            {
                m_Status = UNABLE_MAP_VIEW_OF_FILE;
            }
        }
        else
        {
            // the status is ERROR_ALREADY_EXISTS which is successful
            m_Status = ERROR_SUCCESS;
        }
        // see if Read Only access is required
        if (m_Status == ERROR_SUCCESS)
        {
            // by now the shared memory has already been initialized so
            // we if we don't need write access any more or if it has
            // already been opened, then open with the desired access
            m_pMappedMemory = MapViewOfFile(
                m_hSharedMemory,  // shared mem handle
                (bReadOnlyAccess ? FILE_MAP_READ :
                    FILE_MAP_WRITE),    // access desired
                0,                      // starting offset
                0,
                0);                     // map the entire object
            if (m_pMappedMemory == nullptr)
            {
                m_Status = UNABLE_MAP_VIEW_OF_FILE;
                // this is fatal, if we can't get data then there's no
                // point in continuing.
            }
            else
            {
                m_Status = ERROR_SUCCESS;
            }
        }
    }
}

NamedSharedMemory::~NamedSharedMemory()
{
    if (ERROR_SUCCESS == m_Status)
    {
        if (m_pMappedMemory)
        {
            XPUINFO_REQUIRE(UnmapViewOfFile(m_pMappedMemory));
        }
        if (m_hSharedMemory)
        {
            XPUINFO_REQUIRE(CloseHandle(m_hSharedMemory));
        }
    }
}

NamedPipe::NamedPipe(const String& pipeName, // Must have format "\\\\.\\pipe\\Name"
    const String& mutexName, // Must NOT begin with "\\\\.\\pipe"
    size_t bufferSize,
    bool isServer) : m_bServer(isServer),
    m_bufferSize(bufferSize),
    m_Mutex(mutexName.c_str())
{
    XPUINFO_REQUIRE(m_bufferSize && (m_bufferSize < 0x100000000ULL)); // Check for 32-bit-safe size
    if (!m_bServer)
    {
        // Client-scope lock used to prevent server exit before client completes
        m_pLock.reset(new NamedMutex::ScopedLock(m_Mutex));
        m_hPipe = CreateFileA(pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
    }
    else
    {
        // Create pipe
        m_hPipe = CreateNamedPipeA(pipeName.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, // Single instance
            (DWORD)bufferSize,
            (DWORD)bufferSize,
            PIPE_WAIT,
            nullptr);
    }
}

bool NamedPipe::Connect()
{
    if (!Valid())
    {
        return false;
    }
    if (!m_bConnected)
    {
        bool bRet = ConnectNamedPipe(m_hPipe, nullptr) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        m_bConnected = bRet;
        return bRet;
    }
    else
    {
        return true;
    }
}

bool NamedPipe::Disconnect()
{
    if (m_bConnected && m_bServer)
    {
        NamedMutex::ScopedLock lock(m_Mutex); // Client must be done first
        bool bRet = !!DisconnectNamedPipe(m_hPipe);
        m_bConnected = false; // Play it safe if disconnect failed
        return bRet;
    }
    else
    {
        return true;
    }
}

} // Win
#endif
} // XI
#endif //XPUINFO_USE_IPC
