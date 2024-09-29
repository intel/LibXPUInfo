// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
// These contents may have been developed with support from one or more Intel-operated generative artificial intelligence solutions

// A simple mechanism to send debug messages to the debugger or the console

#pragma once

#include <ostream>
#include <sstream>
#include <iostream>

#ifndef DEBUGSTREAM_DEFAULT_PRINT
#define DEBUGSTREAM_DEFAULT_PRINT false
#endif

namespace XI
{
#ifndef DISABLE_DEBUGSTREAM
#define DISABLE_DEBUGSTREAM 0
#endif

#if DISABLE_DEBUGSTREAM
// Copilot: create class based on std::ostream that does nothing
class NullBuffer : public std::streambuf {
protected:
    // Override the overflow function to discard output
    int overflow(int c) override {
        return c;
    }
};
class NullBufferW : public std::wstreambuf {
protected:
    // Override the overflow function to discard output
    std::wstreambuf::int_type overflow(std::wstreambuf::int_type c) override {
        return c;
    }
};

class NullOStream : public std::ostream {
public:
    NullOStream() : std::ostream(&nullBuffer) {}

private:
    NullBuffer nullBuffer;
};

class NullOStreamW : public std::wostream {
public:
    NullOStreamW() : std::wostream(&nullBuffer) {}

private:
    NullBufferW nullBuffer;
};
#endif

class DebugStream
#if !DISABLE_DEBUGSTREAM
    :public std::ostringstream
#else
    :public NullOStream
#endif
{
public:
    // false or default constructor prints only to debugger when present, true prints to stdout if no debugger
    DebugStream(bool bPrintAlways=DEBUGSTREAM_DEFAULT_PRINT);
    ~DebugStream();
    DebugStream(const DebugStream&) = delete;
    DebugStream& operator=(const DebugStream&) = delete;

    void OutputToDebugger() noexcept;

private:
    bool bPrintAlways_;
};

class DebugStreamW
#if !DISABLE_DEBUGSTREAM
    :public std::wostringstream
#else
    : public NullOStreamW
#endif
{
public:
    DebugStreamW(bool bPrintAlways = DEBUGSTREAM_DEFAULT_PRINT);
    ~DebugStreamW();
    DebugStreamW(const DebugStreamW&) = delete;
    DebugStreamW& operator=(const DebugStreamW&) = delete;

    void OutputToDebugger() noexcept;

private:
    bool bPrintAlways_;
};

#ifdef UNICODE
using DebugStreamT = DebugStreamW;
#else
using DebugStreamT = DebugStream;
#endif

} // XI
