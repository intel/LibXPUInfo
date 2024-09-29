#pragma once

#define BUFSIZE 4096
#define SHARED_MEM_NAME "XPUINFO_IPC_SHAREDMEM"
#define EVENT_NAME "XPUINFO_IPC_EVENT"
#define MUTEX_NAME "XPUINFO_IPC_MUTEX"
#define SEMAPHORE_NAME "XPUINFO_IPC_SEMAPHORE"
#define PIPE_NAME "\\\\.\\pipe\\XPUINFO_IPC"

#ifdef _WIN32
int XPUInfoIPC_Client_Pipe(const char* serverCommandString, PROCESS_INFORMATION& pi);
int XPUInfoIPC_Client(const char* serverCommandString, PROCESS_INFORMATION& pi);
int XPUInfo_IPC_Server_Pipe();
int XPUInfo_IPC_Server();
#endif // _WIN32
