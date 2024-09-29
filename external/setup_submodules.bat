@REM This is for setting up a local build or new git repo from a git archive
@REM This info comes from: git submodule status

@setlocal
set _ROOT=%~dp0\..
pushd %_ROOT%

rmdir external\rapidjson
git submodule add  https://github.com/Tencent/rapidjson.git ./external/rapidjson
@if %ERRORLEVEL% neq 0 goto ERROR
cd external\rapidjson
git checkout 7c73dd7de7c4f14379b781418c6e947ad464c818
cd ..\..

rmdir external\OpenCL-CLHPP
git submodule add https://github.com/KhronosGroup/OpenCL-CLHPP.git ./external/OpenCL-CLHPP
cd external\OpenCL-CLHPP
git checkout 6db44b8db11952b53d271b6d1657ac5d04a45871
cd ..\..

rmdir external\IGCL
git submodule add https://github.com/intel/drivers.gpu.control-library ./external/IGCL
cd external\IGCL
git checkout 5d7c64e45ae47e95deef542f174d8bdc24ee23cf
cd ..\..

rmdir external\level-zero
git submodule add https://github.com/oneapi-src/level-zero ./external/level-zero
cd external\level-zero
git checkout v1.17.42
cd ..\..

rmdir external\OpenCL-Headers
git submodule add https://github.com/KhronosGroup/OpenCL-Headers.git ./external/OpenCL-Headers
cd external\OpenCL-Headers
git checkout 542d7a8f65ecfd88b38de35d8b10aa67b36b33b2
cd ..\..

rmdir external\OpenCL-ICD-Loader
git submodule add https://github.com/KhronosGroup/OpenCL-ICD-Loader.git ./external/OpenCL-ICD-Loader
cd external\OpenCL-ICD-Loader
git checkout v2024.05.08
cd ..\..

git submodule update --init --recursive
goto END
:ERROR
@echo "Error - do you need to set https_proxy?"

:END
git submodule status --recursive

popd
@endlocal
