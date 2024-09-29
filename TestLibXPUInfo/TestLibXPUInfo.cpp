#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef TESTLIBXPUINFO_STANDALONE
#define TESTLIBXPUINFO_STANDALONE 1
#endif
#include "LibXPUInfo.h"
#include "LibXPUInfo_Util.h"
#include "LibXPUInfo_JSON.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace XI;

#ifdef XPUINFO_USE_RAPIDJSON
bool getXPUInfoJSON(std::ostream& ostr, const XI::XPUInfoPtr& pXI)
{
    if (!ostr.fail())
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto& a = doc.GetAllocator();

        if (pXI->serialize(doc, a))
        {
            rapidjson::OStreamWrapper out(ostr);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(out);
            doc.Accept(writer);    // Accept() traverses the DOM and generates Handler events.
            ostr << std::endl;
            return true;
        }
    }
    return false;
}

bool testWriteJSON(const std::filesystem::path jsonPath)
{
    std::filesystem::create_directories(std::filesystem::absolute(jsonPath).parent_path()); // Create if needed
    std::ofstream jf(jsonPath);
    APIType apis = APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI);
    std::cout << "Initializing XPUInfo with APIType = " << apis << "...\n";
    XI::XPUInfoPtr pXI(new XPUInfo(apis));

    bool result = getXPUInfoJSON(jf, pXI);
    if (!result)
    {
        std::cout << "Error writing " << std::filesystem::absolute(jsonPath) << std::endl;
    }
    else
    {
        std::cout << "Wrote " << std::filesystem::absolute(jsonPath) << std::endl;
    }
    return result;
}

bool testVerifyJSON()
{
    std::ostringstream ostr;
    APIType apis = APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI);
    std::cout << "Initializing XPUInfo with APIType = " << apis << "...\n";
    XI::XPUInfoPtr pXI(new XPUInfo(apis));
    if (getXPUInfoJSON(ostr, pXI))
    {
        std::istringstream istr(ostr.str());
        if (!istr.bad())
        {
            rapidjson::IStreamWrapper isw(istr);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            if (doc.HasParseError())
            {
                std::cout << "Error  : " << doc.GetParseError() << ": "
                    << "Offset : " << doc.GetErrorOffset() << '\n';
                return false;
            }
            XI::XPUInfoPtr pXID(XI::XPUInfo::deserialize(doc));
            bool xiEqual = JSON::compareXI(pXI, pXID);
            if (!xiEqual)
            {
                std::cout << "XPUInfo comparison failed!\n";
            }
            else
            {
                std::cout << "XPUInfo comparison matched!\n";
            }
            return xiEqual;
        }
    }
    else
    {
        std::cout << "Failed to get XPUInfo JSON!\n";
    }
    return false;
}

bool testReadJSON(const std::filesystem::path jsonPath)
{
    if (std::filesystem::directory_entry(jsonPath).is_regular_file())
    {
        std::ifstream istr(jsonPath);
        if (!istr.bad())
        {
            rapidjson::IStreamWrapper isw(istr);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            if (doc.HasParseError())
            {
                std::cout << "Error  : " << doc.GetParseError() << ": "
                    << "Offset : " << doc.GetErrorOffset() << '\n';
                return false;
            }
            XI::XPUInfoPtr pXID(XI::XPUInfo::deserialize(doc));
            std::cout << *pXID << std::endl;
            return true;
        }
    }
    else
    {
        std::cout << "File not found: " << std::filesystem::absolute(jsonPath).string() << std::endl;
    }
    return false;
}
#endif

#if TESTLIBXPUINFO_STANDALONE
int main(int argc, char* argv[])
#else
int printXPUInfo(int argc, char* argv[])
#endif
{
    bool testIndividual = false;
    APIType additionalAPIs = APIType(0);
    for (int a = 1; a < argc; ++a)
    {
        String arg(argv[a]);
#ifdef _WIN32
        if (arg == "-1")
        {
            testIndividual = true;
        }
#endif
        if (arg == "-igcl_l0_enable")
        {
            additionalAPIs |= XI::API_TYPE_IGCL_L0;
        }
#ifdef XPUINFO_USE_RAPIDJSON
        if (arg == "-write_json")
        {
            testWriteJSON(argv[++a]);
        }
        if (arg == "-verify_json")
        {
            testVerifyJSON();
        }
        if (arg == "-from_json")
        {
            testReadJSON(argv[++a]);
        }
#endif
    }

    try
    {
        if (!testIndividual)
        {
            XI::Timer timer;
            APIType apis = APIType((XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI \
                | XI::API_TYPE_DX11_INTEL_PERF_COUNTER | XI::API_TYPE_IGCL | XI::API_TYPE_OPENCL \
                | XI::API_TYPE_LEVELZERO \
                | XI::API_TYPE_DXCORE | XI::API_TYPE_NVML) | API_TYPE_WMI);
            apis |= additionalAPIs;
            timer.Start();
            std::cout << "Initializing XPUInfo with APIType = " << apis << "...\n";
            XI::XPUInfo xi(apis);
            std::cout << xi << std::endl;
            timer.Stop();
            std::cout << "XPUInfo Time: " << timer.GetElapsedSecs() << " seconds\n";
        }
        else
        {
            std::vector<APIType> apiVec;
            apiVec.push_back(XPUINFO_INIT_ALL_APIS);  // All but WMI
            apiVec.push_back(APIType(XPUINFO_INIT_ALL_APIS | API_TYPE_WMI));

            // Either DXGI or DXCore is needed in all cases
            apiVec.push_back(APIType(XI::API_TYPE_DXGI | XI::API_TYPE_DX11_INTEL_PERF_COUNTER));
            apiVec.push_back(XI::API_TYPE_DXCORE);
            apiVec.push_back(APIType(XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI));
            apiVec.push_back(APIType(XI::API_TYPE_DXGI | XI::API_TYPE_SETUPAPI | XI::API_TYPE_LEVELZERO));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_SETUPAPI));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_IGCL));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_OPENCL));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_LEVELZERO));
            apiVec.push_back(APIType(XI::API_TYPE_DXCORE | XI::API_TYPE_NVML));

            for (auto api : apiVec)
            {
                std::cout << "Initializing XPUInfo with APIType = " << api << "...\n";
                XI::Timer timer;
                timer.Start();
                XI::XPUInfo xi(api);
                std::cout << xi << std::endl;
                timer.Stop();
                std::cout << "XPUInfo Time: " << timer.GetElapsedSecs() << " seconds\n";
            }
        }
    }
    catch (...)
    {
        std::cout << "Exception initializing XPUInfo!\n";
        return -1;
    }
    return 0;
}
