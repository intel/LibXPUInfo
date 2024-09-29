// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// These contents may have been developed with support from one or more Intel-operated generative artificial intelligence solutions

#ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif
#include "LibXPUInfo.h"
#include <locale>
#include <codecvt>
#include <cwctype>

namespace XI
{
	std::string convert(const std::wstring& wstr)
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.to_bytes(wstr);
	}

	std::wstring convert(const std::string& str)
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes(str);
	}

	std::string toLower(const std::string& s)
	{
		std::string data;
		data.resize(s.size());
		for (size_t i = 0; i < s.size(); ++i)
		{
			data[i] = static_cast<char>(std::tolower(s.data()[i]));
		}
		return data;
	}

	std::wstring toLower(const std::wstring& s)
	{
		std::wstring data;
		data.resize(s.size());
		for (size_t i = 0; i < s.size(); ++i)
		{
			data[i] = std::towlower(s.data()[i]);
		}
		return data;
	}

#ifdef _WIN32
namespace Win
{
String GetLastErrorStr(DWORD dwErr)
{
	String errStr;
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		dwErr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&lpMsgBuf,
		0, nullptr);

	errStr = (LPCSTR)lpMsgBuf; // Copy
	LocalFree(lpMsgBuf);

	return errStr;
}

#pragma comment(lib, "version.lib")

// Copilot: write function to get dll version, modified for std::optional
std::optional<RuntimeVersion> GetDllVersion(const std::string& filePath) {
	RuntimeVersion versionInfo;
	DWORD handle = 0;
	DWORD size = GetFileVersionInfoSizeA(filePath.c_str(), &handle);
	if (size == 0) {
		return std::nullopt;
	}

	std::vector<char> buffer(size);
	if (!GetFileVersionInfoA(filePath.c_str(), handle, size, buffer.data())) {
		return std::nullopt;
	}

	void* verInfo = nullptr;
	UINT verInfoSize = 0;
	if (VerQueryValueA(buffer.data(), "\\", &verInfo, &verInfoSize) && verInfoSize >= sizeof(VS_FIXEDFILEINFO)) {
		VS_FIXEDFILEINFO* fileInfo = static_cast<VS_FIXEDFILEINFO*>(verInfo);
		versionInfo.major = HIWORD(fileInfo->dwProductVersionMS);
		versionInfo.minor = LOWORD(fileInfo->dwProductVersionMS);
		versionInfo.build = HIWORD(fileInfo->dwProductVersionLS);
	}

	if (VerQueryValueA(buffer.data(), "\\StringFileInfo\\040904E4\\ProductVersion", &verInfo, &verInfoSize)) {
		versionInfo.productVersion = std::string(static_cast<char*>(verInfo), verInfoSize - 1);
	}

	return versionInfo;
}

bool GetVersionFromFile(const String& filePath, RuntimeVersion& fileVer)
{
	auto rval = GetDllVersion(filePath);
	if (rval.has_value())
	{
		fileVer = std::move(rval.value());
		return true;
	}
    return false;
}

std::string getDateString(const FILETIME& ft)
{
	SYSTEMTIME stime;
	FileTimeToSystemTime(&ft, &stime);
	std::string dateStr;
	const char fmt[] = "yyyy-MM-dd";
	int rval = GetDateFormatA(LOCALE_INVARIANT, 0, &stime, fmt, nullptr, 0);
	if (rval)
	{
		dateStr.resize(rval - 1); // Don't include null terminator
		rval = GetDateFormatA(LOCALE_INVARIANT, 0, &stime, fmt, dateStr.data(), rval);
	}
	return dateStr;
}

} // Win
#endif // _WIN32

} // XI
