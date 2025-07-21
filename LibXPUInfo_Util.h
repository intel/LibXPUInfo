// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "LibXPUInfo.h"
#ifndef XPUINFO_USE_STD_CHRONO
#ifdef _WIN32
#define XPUINFO_USE_STD_CHRONO 0
#else
#define XPUINFO_USE_STD_CHRONO 1
#endif
#endif

#if XPUINFO_USE_STD_CHRONO
#include <chrono>
#endif

namespace XI
{
#if XPUINFO_USE_STD_CHRONO
typedef std::chrono::high_resolution_clock ClockType;
typedef std::chrono::time_point<ClockType> TimerTickValueType;
typedef std::chrono::nanoseconds TimerDuration;
#else
typedef UI64 TimerTickValueType;
typedef UI64 TimerDuration;
#endif

    template<typename T>
    XI::UI64 reinterpretAsUI64(const T& x)
    {
        return *reinterpret_cast<const XI::UI64*>(&x);
    }

	template <typename T>
	void updateIfNotZero(T& dst, const T& src)
	{
		if (src != T(0))
		{
			dst = src;
		}
	}

	template <typename T>
	void updateIfDstNotSet(T& dst, const T& src)
	{
		if (dst == T(-1))
		{
			dst = src;
		}
	}

	template <typename T>
	void updateIfDstVal(T& dst, const T& isVal, const T& src)
	{
		if (dst == isVal)
		{
			dst = src;
		}
	}

	template <typename T>
	bool isValidPCIAddr(const T& addr)
	{
		using namespace XI;
		I32 vcnt = 0;
		vcnt += (I32(addr.domain) > 0);
		vcnt += (I32(addr.bus) > 0);
		vcnt += (I32(addr.device) > 0);
		vcnt += (I32(addr.function) > 0);

		return (vcnt > 0) &&
			(addr.domain != -1) &&
			(addr.bus != -1) &&
			(addr.device != -1) &&
			(addr.function != -1);
	}

    XPUINFO_EXPORT std::string convert(const std::wstring& inWStr);
    XPUINFO_EXPORT std::wstring convert(const std::string& str);
    XPUINFO_EXPORT std::string toLower(const std::string& s);
    XPUINFO_EXPORT std::wstring toLower(const std::wstring& s);

    typedef TimerTickValueType TimerTick;

    class XPUINFO_EXPORT TimeInterval
    {
    public:
        TimeInterval() : tStart{}, tEnd{} {}
        TimeInterval(TimerTick t0, TimerTick t1) : tStart(t0), tEnd(t1) {}
        TimerTick tStart;
        TimerTick tEnd;
        TimerDuration operator()() const { return tEnd - tStart; }
    };

    class XPUINFO_EXPORT Timer
    {
    public:
        static TimerTick GetNow()
        {
#if !XPUINFO_USE_STD_CHRONO
            TimerTick tcur;
            QueryPerformanceCounter((LARGE_INTEGER*)&tcur);
            return tcur;
#else
            return ClockType::now();
#endif
        }

        static TimerDuration GetScale()
        {
#if !XPUINFO_USE_STD_CHRONO
            TimerTick tFreq;
            QueryPerformanceFrequency((LARGE_INTEGER*)&tFreq); // counts per sec
            return tFreq;
#else
            using namespace std::literals;
            return std::chrono::nanoseconds(1s);
#endif
        }

        Timer() : mStart{}, mTotal{}
        {
            mTimerFrequency = Timer::GetScale();
#if !XPUINFO_USE_STD_CHRONO
            mInvTimerFrequency = mTimerFrequency ? 1.0 / mTimerFrequency : 0.;
#else
            mInvTimerFrequency = mTimerFrequency.count() ? 1.0 / mTimerFrequency.count() : 0.;
#endif
        }

        void Start()
        {
            mStart = GetNow();
        }

        void Stop()
        {
            TimerTick tcur = GetNow();
            auto t = tcur - mStart;
            mTotal = mTotal + t;
#if !XPUINFO_USE_STD_CHRONO
            mStart = 0;
#endif
        }

        void Reset()
        {
#if !XPUINFO_USE_STD_CHRONO
            mTotal = 0;
            mStart = 0;
#else
            mTotal = mTotal.zero();
            //TODO: Needed?
            //mStart = 0;
#endif
        }

        void ResetAndStart()
        {
            Reset();
            Start();
        }

        TimeInterval GetInterval() {
            TimerTick tcur = GetNow();
            return TimeInterval(mStart, tcur);
        }

        double GetElapsedSecs()
        {
#if !XPUINFO_USE_STD_CHRONO
            return mTotal * mInvTimerFrequency;
#else
            return mTotal.count() * mInvTimerFrequency;
#endif
        }

        static double GetIntervalSecs(const TimeInterval& i)
        {
            return double(i() / Timer::GetScale());
        }
        TimerTick GetStart() const { return mStart; }
    private:
        TimerTick mStart;
        TimerDuration mTotal;
        TimerDuration mTimerFrequency;
        double mInvTimerFrequency;
    };

#ifdef _WIN32
namespace Win
{
    XPUINFO_EXPORT String GetLastErrorStr(DWORD dwErr);
    XPUINFO_EXPORT bool GetVersionFromFile(const String& filePath, RuntimeVersion& fileVer);
    XPUINFO_EXPORT std::string getDateString(const FILETIME& ft);
}
#endif // _WIN32

class L0_Extensions : public std::vector<ze_driver_extension_properties_t>
{
public:
    L0_Extensions(size_t inSize);

    const ze_driver_extension_properties_t* find(const char* inExtName) const;
};

template<
    class CharT,
    class Traits = std::char_traits<CharT>
>
class SaveRestoreIOSFlags
{
public:
    typedef std::basic_ios<CharT, Traits> Stream;
    SaveRestoreIOSFlags(Stream& s): m_stream(s), m_Flags(s.flags()), m_precision(s.precision()) {}
    ~SaveRestoreIOSFlags()
    {
        m_stream.precision(m_precision);
        m_stream.setf(m_Flags, static_cast<std::ios_base::fmtflags>(-1)); // set to original flags
    }

protected:
    Stream& m_stream;
    const std::ios_base::fmtflags m_Flags;
    std::streamsize m_precision;
};

} // XI
