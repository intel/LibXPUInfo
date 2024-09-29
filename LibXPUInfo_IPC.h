// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Utility code for using XPUInfo out-of-process via Inter-Process Communication (IPC)
// 
// NOTE: These IPC classes are intended to provide some improved readability, structure, 
// and exception-safety over plain C calling the OS-provided IPC APIs directly.  It is the 
// APPLICATION'S responsibility to implement IPC in a way that is free of race conditions,
// deadlocks, etc.

#pragma once
#ifdef XPUINFO_USE_IPC
#include "LibXPUInfo.h"
#ifdef _WIN32
#include <Windows.h>
#endif
namespace XI
{
#ifdef _WIN32
namespace Win
{
    // Wrapper for use with CreateProcess
    class ProcessInformation : public PROCESS_INFORMATION, NoCopyAssign
    {
    public:
        ProcessInformation() : PROCESS_INFORMATION{} {} // zero-initialize
        ~ProcessInformation(); // Close non-zero handles
    };

    class NamedEvent: public NoCopyAssign
    {
    public:
        NamedEvent(const char* sharedName);

        void Set()
        {
            SetEvent(m_hEvent);
        }
        UI32 Wait(UI32 timeout = INFINITE)
        {
            return WaitForSingleObject(m_hEvent, timeout);
        }
        ~NamedEvent()
        {
            if (m_hEvent)
            {
                CloseHandle(m_hEvent);
            }
        }
    protected:
        HANDLE m_hEvent;
    };

    class NamedMutex : public NoCopyAssign
    {
    public:
        NamedMutex(const char* sharedName);
        ~NamedMutex()
        {
            if (m_hMutex)
            {
                CloseHandle(m_hMutex);
            }
        }

        class ScopedLock : public NoCopyAssign
        {
        public:
            ScopedLock(NamedMutex& m, UI32 timeout = INFINITE, UI32* pResult = nullptr);
            ~ScopedLock()
            {
                if (m_Result != WAIT_FAILED)
                {
                    ReleaseMutex(m_Mutex.m_hMutex);
                }
            }

        protected:
            NamedMutex& m_Mutex;
            UI32 m_Result;
            UI32* m_pOutResult;
        };
        friend class ScopedLock;
    protected:
        HANDLE m_hMutex;
        UI32 m_CreateError;
    };

    class NamedSemaphore : public NoCopyAssign
    {
    public:
        NamedSemaphore(const char* name, const I32 initialCount);
        ~NamedSemaphore()
        {
            if (m_hSem)
            {
                CloseHandle(m_hSem);
            }
        }

        friend class ScopedAcquire;
        class ScopedAcquire
        {
        public:
            ScopedAcquire(NamedSemaphore& sem, UI32 timeout = INFINITE);
            ~ScopedAcquire();
        protected:
            NamedSemaphore& m_sem;
            UI32 m_Result;
        };
    protected:
        HANDLE m_hSem;
        UI32 m_CreateError;
    };

    class NamedSharedMemory : public NoCopyAssign
    {
    public:
        NamedSharedMemory(size_t size, const char* sharedName, bool bReadOnlyAccess = false);
        ~NamedSharedMemory();

        UI32 getStatus() const { return m_Status; }
        NamedMutex& getMutex() { return m_hMutex; }
        void* getSharedMemPtr() { 
            XPUINFO_REQUIRE(m_pMappedMemory);
            return m_pMappedMemory; 
        }
        size_t size() const { return m_Size; }

    protected:
        const size_t m_Size;
        HANDLE m_hSharedMemory;
        NamedMutex m_hMutex;
        void* m_pMappedMemory = nullptr;
        UI32 m_Status = UI32(-1);
    };

    class NamedPipe : public NoCopyAssign
    {
    public:
        NamedPipe(const String& pipeName, // Must have format "\\\\.\\pipe\\Name"
            const String& mutexName, // Must NOT begin with "\\\\.\\pipe"
            size_t bufferSize,
            bool isServer = false);
        ~NamedPipe()
        {
            if (Valid())
            {
                XPUINFO_REQUIRE(Disconnect());
                XPUINFO_REQUIRE(CloseHandle(m_hPipe));
            }
        }

        bool Valid() const { return (m_hPipe != INVALID_HANDLE_VALUE) && (m_hPipe != 0); }
        bool Connected() const { return Valid() && (!m_bServer || m_bConnected); }

        bool Connect();
        bool Disconnect();

        template <typename T>
        bool Read(T& value)
        {
            XPUINFO_REQUIRE(Connected());
            DWORD bytesRead = 0;
            return ((ReadFile(m_hPipe, &value, sizeof(value), &bytesRead, nullptr) != FALSE) &&
                (bytesRead == sizeof(value)));
        }
        template <>
        bool Read(String& outString)
        {
            XPUINFO_REQUIRE(Connected());
            outString.resize(m_bufferSize);
            DWORD bytesRead = 0;
            BOOL bRet = ReadFile(m_hPipe, outString.data(), (DWORD)outString.size(), &bytesRead, nullptr);
            outString[std::min(bytesRead, DWORD(m_bufferSize - 1))] = 0; // Just in case
            outString.resize(bytesRead);
            return !!bRet;
        }
        template <typename T>
        bool Write(const T& value)
        {
            XPUINFO_REQUIRE(Connected());
            DWORD bytesWritten = 0;
            return ((WriteFile(m_hPipe, &value, sizeof(value), &bytesWritten, nullptr) != FALSE) &&
                (bytesWritten == sizeof(value)));
        }
        template <>
        bool Write(const String& inString)
        {
            XPUINFO_REQUIRE(Connected());
            DWORD bytesWritten = 0;
            BOOL bRet = WriteFile(m_hPipe, inString.data(), 
                (DWORD)std::min(inString.length(), m_bufferSize),
                &bytesWritten, nullptr);
            return bRet && (bytesWritten == inString.length()); // Error if string longer than buffer
        }
    protected:
        HANDLE m_hPipe;
        bool m_bConnected = false;
        const bool m_bServer;
        const size_t m_bufferSize;
        NamedMutex m_Mutex;
        std::unique_ptr<NamedMutex::ScopedLock> m_pLock;
    };
} // Win
#endif // _WIN32
} // XI
#endif // XPUINFO_USE_IPC
