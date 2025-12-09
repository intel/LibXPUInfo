// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// A simple mechanism to send debug messages to the debugger or the console

#include "DebugStream.h"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x501 // WinXP or greater
#endif
#include <windows.h>

#if (_WIN32_WINNT < 0x501)
#error("Must set _WIN32_WINNT >= 0x501")
#endif
#endif // _WIN32

namespace XI
{
void DebugStream::OutputToDebugger() noexcept
{
#if !DISABLE_DEBUGSTREAM
    try
    {
#ifdef _WIN32
        if (IsDebuggerPresent())
        {
            OutputDebugStringA(str().c_str());
        }
        else
#else
        if (1)
        {
            std::cerr << str().c_str();
        }
        else
#endif

            if (bPrintAlways_)
            {
                std::cout << str().c_str();
            }
#ifdef _WIN32
        clear();
#endif
    }
    catch (...)
    {
    }
#endif // DISABLE_DEBUGSTREAM
}

DebugStream::DebugStream(bool bPrintAlways)
: bPrintAlways_(bPrintAlways)
{
}

DebugStream::~DebugStream()
{
    OutputToDebugger();
}


void DebugStreamW::OutputToDebugger() noexcept
{
#if !DISABLE_DEBUGSTREAM
    try
    {
#ifdef _WIN32
        if (IsDebuggerPresent())
        {
            OutputDebugStringW(str().c_str());
        }
        else
#else
        if (1)
        {
            std::wcerr << str().c_str();
        }
        else
#endif

            if (bPrintAlways_)
            {
                std::wcout << str().c_str();
            }
#ifdef _WIN32
        clear();
#endif
    }
    catch (...)
    {
    }
#endif // DISABLE_DEBUGSTREAM
}

DebugStreamW::DebugStreamW(bool bPrintAlways)
    : bPrintAlways_(bPrintAlways)
{
}

DebugStreamW::~DebugStreamW()
{
    OutputToDebugger();
}

} // XI
