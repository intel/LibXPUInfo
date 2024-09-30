# LibXPUInfo
LibXPUInfo coalesces multiple APIs to provide multi-vendor, cross-platform device information in support of optimized device-selection by applications.

[Click here for LibXPUInfo license](LICENSE.txt).

## Platforms (all 64-bit only)
* Supported:
  * Windows 10+ (Primary target)
* Not currently supported:
  * MacOS X 12+ x64/arm64. (CPU, System, and Metal GPUs only, no explicit NPU information)
  * Linux, tested on Ubuntu 20.04. (Limited system and CPU info)

## Build Instructions
* Prerequisites:
  * LibXPUInfo targets C++14 or greater build environments. 
  * Install CMake with it in your path.
  * Using the Visual Studio Installer, if not already present, add component "C++ x64/x86 Spectre-mitigated libs" for the version of the compiler you are using (i.e. "MSVC v143 - VS 2022")
    * Or, remove lines from external/level-zero/CmakeLists.txt that add /Qspectre.  Try external\l0_removeSpectre.bat to perform this patch.

* If you want to accept the licenses of all git submodule dependencies, clone this repository with _--recurse-submodules_ to get dependencies which will be placed in folder _external/_.  Otherwise, modify .gitmodules to remove unwanted dependencies, and remove the corresponding LIBXPUINFO_USE_* preprocessor definitions.
  * Git submodules
    * Intel Graphics Control Library (IGCL) - [License](https://github.com/intel/drivers.gpu.control-library/blob/master/License.txt)
	* Level Zero Loader - [License](https://github.com/oneapi-src/level-zero/blob/master/LICENSE)
      * See above note regarding Spectre-mitigated libs
	* OpenCL-CLHPP - [License](https://github.com/KhronosGroup/OpenCL-CLHPP/blob/main/LICENSE.txt)
	* OpenCL-Headers - [License](https://github.com/KhronosGroup/OpenCL-Headers/blob/main/LICENSE)
	* OpenCL-ICD-Loader - [License](https://github.com/KhronosGroup/OpenCL-ICD-Loader/blob/main/LICENSE)
	* RapidJSON - [License](https://github.com/Tencent/rapidjson/blob/master/license.txt)
      * If not using RapidJSON, remove XPUINFO_USE_RAPIDJSON from LibXPUInfo project and all projects using LibXPUInfo.

* Windows
  * If you want to use nVidia NVML and accept [NVML license](https://developer.download.nvidia.com/compute/cuda/redist/cuda_nvml_dev/LICENSE.txt), run external\getNVMLDep.bat
    * If you do not want to use NVML, remove XPUINFO_USE_NVML from preprocessor arguments for LibXPUInfo.vcxproj
  * If you want to use OpenCL and accept related Apache-2.0 licenses, run external/buildExternalDeps_OCL.bat
  * If you want to use Level Zero and accept related MIT license, run external/buildExternalDeps_L0.bat
    * See above note regarding Spectre-mitigated libs
  * Open LibXPUInfo.sln and build
    * Note: Modify XPUINFO_USE_* preprocessor flags as desired 
 * MacOS/Linux - Not currently supported. No build config files provided. Use at your own risk - information provided may be incorrect.
