@setlocal

@set _CMAKEGEN="Visual Studio 17 2022"
set _EXTPATH=%~dp0

@where cmake
@if %ERRORLEVEL% neq 0 (
    echo cmake not found!
    goto ERROR
)

cmake -D BUILD_TESTING=OFF -D CMAKE_INSTALL_PREFIX=%_EXTPATH%/OpenCL-Headers-install -S %_EXTPATH%/OpenCL-Headers -B %_EXTPATH%/OpenCL-Headers-build -G %_CMAKEGEN%  -A x64
if %ERRORLEVEL% neq 0 goto ERROR

cmake --build %_EXTPATH%/OpenCL-Headers-build --target install
if %ERRORLEVEL% neq 0 goto ERROR

cmake -D CMAKE_PREFIX_PATH=%~dp0/OpenCL-Headers-install -D CMAKE_INSTALL_PREFIX=%_EXTPATH%/OpenCL-ICD-Loader-install -S %_EXTPATH%/OpenCL-ICD-Loader -B %_EXTPATH%/OpenCL-ICD-Loader-build -G %_CMAKEGEN% -A x64
if %ERRORLEVEL% neq 0 goto ERROR

cmake --build %_EXTPATH%/OpenCL-ICD-Loader-build --target install --config RelWithDebInfo
@REM -- /property:ContinueOnError=true
if %ERRORLEVEL% neq 0 goto ERROR

goto END
:ERROR
@echo FAILED!
@exit /b -1

:END
@endlocal
