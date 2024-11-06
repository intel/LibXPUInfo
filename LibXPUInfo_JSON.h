// Copyright (C) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifdef XPUINFO_USE_RAPIDJSON
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h" // for XI::convert
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/istreamwrapper.h>
// Support serialization
#include <optional>

#define XPUINFO_JSON_VERSION "0.0.1"

namespace XI
{
namespace JSON
{
    template <typename T>
    const char* safeGetValString(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsString())
            {
                return val[valName].GetString();
            }
        }
        return nullptr;
    }

    template <typename T>
    std::string safeGetString(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsString())
            {
                return val[valName].GetString();
            }
        }
        return std::string();
    }

    template <typename T>
    std::wstring safeGetWString(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsString())
            {
                return convert(val[valName].GetString());
            }
        }
        return std::wstring();
    }

    template <typename T>
    std::optional<XI::UI64> safeGetUI64(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsUint64())
            {
                return val[valName].GetUint64();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<XI::UI64> safeGetI64(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsInt64())
            {
                return val[valName].GetInt64();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<XI::UI32> safeGetUI32(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsUint())
            {
                return val[valName].GetUint();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<XI::I32> safeGetI32(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsInt())
            {
                return val[valName].GetInt();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<double> safeGetDouble(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsDouble())
            {
                return val[valName].GetDouble();
            }
        }
        return std::nullopt;
    }

    template <typename T>
    std::optional<bool> safeGetBool(const T& val, const char* valName)
    {
        if (val.HasMember(valName))
        {
            if (val[valName].IsBool())
            {
                return val[valName].GetBool();
            }
        }
        return std::nullopt;
    }

    // For validating original vs. deserialized objects
    bool XPUINFO_EXPORT compareXI(const XPUInfoPtr& pXI, const XPUInfoPtr& pXID);

} // JSON

} // XI
#endif // XPUINFO_USE_RAPIDJSON
