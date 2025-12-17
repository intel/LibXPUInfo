// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

/* IPC Test - XPUInfo initialized on server, client checks success
* 
* There are 2 paths here - pipe and shared-memory.  The pipe path is simpler and preferred.
* 
* Shared-memory path:
*  - Client process launches server process, Server waits for first signal
*  - Client initializes shared memory
*  - Client passes "initMask" through shared memory and signals completion
*  - Server receives initMask, initializes XPUInfo, writes response to shared memory, and exits
*  - Client waits for server process completion, checks exit result code, and displays result. 
*/
#include "LibXPUInfo.h"
#include "LibXPUInfo_IPC.h"
#include "TestXPUInfoIPC_Shared.h"
#include "LibXPUInfo_Util.h"
#include "LibXPUInfo_JSON.h"
#include <iostream>
#include <sstream>
#include <fstream>

#ifdef TESTXPUINFOIPC_SHARED
#if TESTXPUINFOIPC_SUPPORT_PIPE
int XPUInfoIPC_Client_Pipe(const char* serverCommandString, PROCESS_INFORMATION& pi)
{
    // This event is used for our handshake/protocol
    static XI::Win::NamedEvent S_Event(EVENT_NAME);

    DWORD exitCode = 0;
    STARTUPINFOA si{};
    si.cb = sizeof(si);

    BOOL bRet = CreateProcessA(NULL,
        const_cast<char*>(serverCommandString),
        NULL, NULL,
        TRUE,
        CREATE_SUSPENDED,
        NULL, NULL,
        &si, &pi);
    XPUINFO_REQUIRE(bRet);
    int res = ResumeThread(pi.hThread);
    XPUINFO_REQUIRE(res != -1);
    if (bRet)
    {
        // Start by waiting for server to signal ready
        std::cout << "[CLIENT] Waiting..." << std::endl;
        auto waitRes = S_Event.Wait();
        XPUINFO_REQUIRE(waitRes == WAIT_OBJECT_0);

        std::cout << "[CLIENT] Opening Pipe..." << std::endl;
        { // Pipe scope
            // Open pipe
            XI::Win::NamedPipe Pipe(PIPE_NAME, MUTEX_NAME, BUFSIZE);
            if (Pipe.Valid())
            {
                XI::APIType initMask = XPUINFO_INIT_ALL_APIS | XI::API_TYPE_WMI;
                std::cout << "[CLIENT] Pipe opened, writing initMask = " << initMask << std::endl;

                bRet = Pipe.Write(initMask);
                XPUINFO_REQUIRE(bRet);
                std::cout << "[CLIENT] Wrote " << sizeof(initMask) << " bytes to pipe\n";

                // Read pipe
                std::cout << "[CLIENT] Reading from pipe...\n";
                XI::String buffer;
                bRet = Pipe.Read(buffer);
                if (!bRet)
                {
                    auto gle = GetLastError();
                    std::cout << "[CLIENT] Pipe Read Error " << gle << ": " << XI::Win::GetLastErrorStr(gle);
                    XPUINFO_REQUIRE(bRet);
                }
                std::cout << "[CLIENT] Read " << buffer.size() << " from pipe:\n" << buffer;
            }
        } // end pipe scope

        // Validate server exit
        std::cout << "[CLIENT] Waiting for server to exit..." << std::endl;
        DWORD dwRes = WaitForSingleObject(pi.hProcess, INFINITE);
        if (dwRes == WAIT_OBJECT_0)
        {
            bRet = GetExitCodeProcess(pi.hProcess, &exitCode);
            XPUINFO_REQUIRE(bRet);
            std::cout << "[CLIENT] Subprocess returned " << exitCode << std::endl;
        }
    }
    return exitCode;
}

int XPUInfo_IPC_Server_Pipe()
{
    // This semaphore limits server execution to 1 client at a time, 
    // so if multiple clients are trying to run multiple server processes, 
    // each server process will service just one client.
    static XI::Win::NamedSemaphore S_SingleExecSemaphore(SEMAPHORE_NAME, 1);
    // This event is used for our handshake/protocol
    static XI::Win::NamedEvent S_Event(EVENT_NAME);

    std::cout << "[SERVER] Initializing XPUInfo Server (Pipe)...\n";
    try
    {
        XI::Win::NamedSemaphore::ScopedAcquire lock(S_SingleExecSemaphore);
        int exitCode = 0;

        XI::Win::NamedPipe Pipe(PIPE_NAME, MUTEX_NAME, BUFSIZE, true);
        // Signal ready, regardless of status so client is not stuck
        S_Event.Set();
        if (Pipe.Valid())
        {
            std::cout << "[SERVER] Pipe Created\n";

            if (Pipe.Connect())   // wait for someone to connect to the pipe
            {
                // Read pipe to get initMask
                XI::APIType initMask = XI::APIType::API_TYPE_UNKNOWN;

                if (Pipe.Read(initMask))
                {
                    std::cout << "[SERVER] Read " << sizeof(initMask) << " bytes from pipe:\tinitMask = " << initMask << std::endl;

                    XI::XPUInfo xi(initMask);

                    std::ostringstream str;
                    str << xi << std::endl;

                    if (Pipe.Write(str.str()))
                    {
                        std::cout << "[SERVER] Wrote " << str.str().length() << " bytes to pipe\n";
                    }
                    else
                    {
                        std::cout << "[SERVER] Pipe write failed!\n";
                    }
                    XPUINFO_REQUIRE(Pipe.Disconnect());
                }
                else
                {
                    XPUINFO_REQUIRE_MSG(0, "Failed to read initMask");
                }
            }
            else
            {
                auto lastErr = GetLastError();
                std::cout << "[SERVER] ConnectNamedPipe failed with " << lastErr << ": " << XI::Win::GetLastErrorStr(lastErr);
                return lastErr;
            }
        }
        else
        {
            DWORD err = exitCode = GetLastError();
            std::cout << "[SERVER] CreateNamedPipeA failed with result = " << err << ": " << XI::Win::GetLastErrorStr(err);
        }
        return exitCode;
    }
    catch (...)
    {
        std::cout << "Exception initializing XPUInfo!\n";
        return -1;
    }
}
#endif // TESTXPUINFOIPC_SUPPORT_PIPE

#if TESTXPUINFOIPC_SUPPORT_SHAREDMEM
int XPUInfoIPC_Client(const char* serverCommandString, PROCESS_INFORMATION& pi)
{
    // This event is used for our handshake/protocol
    static XI::Win::NamedEvent S_Event(EVENT_NAME);

    DWORD exitCode = 0;
    STARTUPINFOA si{};
    si.cb = sizeof(si);

    BOOL bRet = CreateProcessA(NULL,
        const_cast<char*>(serverCommandString),
        NULL, NULL,
        TRUE,
        CREATE_SUSPENDED,
        NULL, NULL,
        &si, &pi);
    XPUINFO_REQUIRE(bRet);
    int res = ResumeThread(pi.hThread);
    XPUINFO_REQUIRE(res != -1);
    if (bRet)
    {
        XI::Win::NamedSharedMemory SharedMem(BUFSIZE, SHARED_MEM_NAME);
        XPUINFO_REQUIRE(!SharedMem.getStatus());
        XI::APIType initMask = XPUINFO_INIT_ALL_APIS;
        std::cout << "Setting initMask = " << std::hex << initMask << std::dec << std::endl;
        {
            XI::Win::NamedMutex::ScopedLock lock(SharedMem.getMutex());
            *(XI::APIType*)SharedMem.getSharedMemPtr() = initMask;
        }
        S_Event.Set();

        std::cout << "Waiting..." << std::endl;
        DWORD dwRes = WaitForSingleObject(pi.hProcess, INFINITE);
        if (dwRes == WAIT_OBJECT_0)
        {
            bRet = GetExitCodeProcess(pi.hProcess, &exitCode);
            XPUINFO_REQUIRE(bRet);
            {
                XI::Win::NamedMutex::ScopedLock lock(SharedMem.getMutex());
                const char* str = (const char*)SharedMem.getSharedMemPtr();
                if (bRet)
                {
                    std::cout << "Subprocess returned " << exitCode << ", output:\n" << str << std::endl;
                }
            }
        }
    }
    return exitCode;
}

int XPUInfo_IPC_Server()
{
    // This semaphore limits server execution to 1 client at a time, 
    // so if multiple clients are trying to run multiple server processes, 
    // each server process will service just one client.
    static XI::Win::NamedSemaphore S_SingleExecSemaphore(SEMAPHORE_NAME, 1);
    // This event is used for our handshake/protocol
    static XI::Win::NamedEvent S_Event(EVENT_NAME);

    std::cout << "Initializing XPUInfo Server...\n";
    try
    {
        XI::Win::NamedSemaphore::ScopedAcquire lock(S_SingleExecSemaphore);
        XI::Win::NamedSharedMemory SharedMem(BUFSIZE, SHARED_MEM_NAME);
        XPUINFO_REQUIRE(!SharedMem.getStatus());

        // Wait for data received before exiting.
        auto dwRes = S_Event.Wait();
        XI::APIType initMask = XI::APIType::API_TYPE_UNKNOWN;
        if (dwRes == WAIT_OBJECT_0)
        {
            {
                XI::Win::NamedMutex::ScopedLock lockSharedMem(SharedMem.getMutex());
                initMask = *(XI::APIType*)SharedMem.getSharedMemPtr();
                std::cout << "Received initMask = " << std::hex << initMask << std::dec << std::endl;
            }
        }

        std::ostringstream str;
        XI::XPUInfo xi(initMask);
        str << xi << std::endl;

        {
            XI::Win::NamedMutex::ScopedLock lockSharedMem(SharedMem.getMutex());
            if (!SharedMem.getStatus())
            {
                char* sharedString = (char*)SharedMem.getSharedMemPtr();
                strncpy_s(sharedString, SharedMem.size(), str.str().c_str(), _TRUNCATE);
            }
        }
        return 0;
    }
    catch (...)
    {
        std::cout << "Exception initializing XPUInfo!\n";
        return -1;
    }
}
#endif // TESTXPUINFOIPC_SUPPORT_SHAREDMEM
#else // TESTXPUINFOIPC_SHARED

#ifdef XPUINFO_USE_RAPIDJSON
bool getXPUInfoJSON(std::ostream& ostr, const XI::XPUInfoPtr& pXI, double xiTime)
{
    if (!ostr.fail())
    {
        rapidjson::Document doc;
        doc.SetObject();

        if (pXI->serialize(doc))
        {
            auto& a = doc.GetAllocator();
            doc.AddMember("XPUInfoInitSecs", xiTime, a);
            rapidjson::OStreamWrapper out(ostr);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(out);
            doc.Accept(writer);    // Accept() traverses the DOM and generates Handler events.
            ostr << std::endl;
            return true;
        }
    }
    return false;
}

int writeXPUInfoJSON(const char* jsonPath, const XI::RuntimeNames& runtimes, const XI::String& clientTimestamp,
    const XI::APIType apis = XPUINFO_INIT_ALL_APIS | XI::API_TYPE_WMI)
{
    XI::Timer timer;
    timer.Start();
    XI::XPUInfoPtr pXI = clientTimestamp.empty() ? std::make_shared<XI::XPUInfo>(apis, runtimes) :
        std::make_shared<XI::XPUInfo>(apis, runtimes, sizeof(XI::XPUInfo), clientTimestamp.c_str());
    timer.Stop();
    double xiTime = timer.GetElapsedSecs();
    std::ofstream outFile(jsonPath);
    return (!getXPUInfoJSON(outFile, pXI, xiTime));
}
#endif // XPUINFO_USE_RAPIDJSON

namespace // private
{
std::vector<std::string> parseCommaSeparatedList(const std::string& input) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;

    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }

    return result;
}
} // private

int main(int argc, char* argv[])
{
    int procRetVal = -1;
#ifdef _DEBUG
    std::cout << argv[0] << " ";
    for (int a = 1; a < argc; ++a)
    {
        std::cout << argv[a] << " ";
    }
    std::cout << std::endl;
#endif

#ifdef _WIN32
    try
    {
        bool isServer = false;
        bool usePipe = true;
        XI::RuntimeNames runtimes;
        XI::APIType apis = XPUINFO_INIT_ALL_APIS | XI::API_TYPE_WMI;
        XI::String clientTimestamp;
        for (int a = 1; a < argc; ++a)
        {
            std::string arg(argv[a]);
#ifdef XPUINFO_USE_RAPIDJSON
            // Usage: -write_json outfileName apiMask
            if (arg == "-write_json")
            {
                XPUINFO_REQUIRE(a + 1 < argc);
                return writeXPUInfoJSON(argv[++a], runtimes, clientTimestamp, apis);
            }
            else
#endif
            // Usage: -apis apiMaskHex
            // Put this arg before -write_json or others using it
            if (arg == "-apis")
            {
                XPUINFO_REQUIRE(a + 1 < argc);
                std::underlying_type_t<XI::APIType> inMask;
                std::istringstream istr(argv[++a]);
                istr >> std::hex >> inMask;
                if (!istr.fail())
                {
                    apis = static_cast<XI::APIType>(inMask);
                }
            }
            else if (arg == "-runtimes")
            {
                // Read runtime names as comma-separated list
                if ((a + 1) < argc)
                {
                    runtimes = parseCommaSeparatedList(argv[a + 1]);
                }
            }
            else if ((arg == "-client_timestamp") && (a + 1 < argc))
            {
                clientTimestamp = argv[++a];
            }
            else if (arg == "-server")
            {
                isServer = true;
            }
            else if (arg == "-sharedmem")
            {
                usePipe = false;
            }
        } // for each arg

        XPUINFO_REQUIRE_CONSTEXPR_MSG(TESTXPUINFOIPC_SUPPORT_PIPE || TESTXPUINFOIPC_SUPPORT_SHAREDMEM,
            "Must build with TESTXPUINFOIPC_SUPPORT_PIPE or TESTXPUINFOIPC_SUPPORT_SHAREDMEM");
#if TESTXPUINFOIPC_SUPPORT_PIPE || TESTXPUINFOIPC_SUPPORT_SHAREDMEM
        if (isServer)
        {
            if (usePipe)
            {
                XPUINFO_REQUIRE_CONSTEXPR_MSG(TESTXPUINFOIPC_SUPPORT_PIPE,
                    "Must build with TESTXPUINFOIPC_SUPPORT_PIPE");
#if TESTXPUINFOIPC_SUPPORT_PIPE
                procRetVal = XPUInfo_IPC_Server_Pipe();
#endif
            }
            else
            {
                XPUINFO_REQUIRE_CONSTEXPR_MSG(TESTXPUINFOIPC_SUPPORT_SHAREDMEM,
                    "Must build with TESTXPUINFOIPC_SUPPORT_SHAREDMEM");
#if TESTXPUINFOIPC_SUPPORT_SHAREDMEM
                procRetVal = XPUInfo_IPC_Server();
#endif
            }
        }
        else
        {
            char szFileName[MAX_PATH];

            auto sizeRet = GetModuleFileNameA(NULL, szFileName, MAX_PATH);
            auto lastErr = GetLastError();
            if ((lastErr != ERROR_INSUFFICIENT_BUFFER) && sizeRet)
            {
                std::cout << "Launching server process: " << szFileName << std::endl;

                std::string args;
                args.reserve(32767); // See CreateProcess docs - unclear if this is required
                args = szFileName;
                args += " -server";
                XI::Win::ProcessInformation pi;
                if (!usePipe)
                {
                    XPUINFO_REQUIRE_CONSTEXPR_MSG(TESTXPUINFOIPC_SUPPORT_SHAREDMEM,
                        "Must build with TESTXPUINFOIPC_SUPPORT_SHAREDMEM");
                    args += " -sharedmem";
#if TESTXPUINFOIPC_SUPPORT_SHAREDMEM
                    procRetVal = XPUInfoIPC_Client(args.c_str(), pi);
#endif
                }
                else
                {
                    XPUINFO_REQUIRE_CONSTEXPR_MSG(TESTXPUINFOIPC_SUPPORT_PIPE,
                        "Must build with TESTXPUINFOIPC_SUPPORT_PIPE");
#if TESTXPUINFOIPC_SUPPORT_PIPE
                    procRetVal = XPUInfoIPC_Client_Pipe(args.c_str(), pi);
#endif
                }
            }
            else
            {
                std::cout << "Error from GetModuleFileNameA, size returned = " << sizeRet << std::endl;
            }
        } // End Client
        std::cout << (isServer ? "[SERVER] " : "[CLIENT] ") << "Exiting with code " << procRetVal << std::endl;
#endif // TESTXPUINFOIPC_SUPPORT_PIPE || TESTXPUINFOIPC_SUPPORT_SHAREDMEM
    }
    catch (std::logic_error& e)
    {
        std::cout << "Caught exception: " << e.what() << std::endl;
        procRetVal = 0;
    }
#endif // _WIN32
    return procRetVal;
}

#endif // TESTXPUINFOIPC_SHARED
