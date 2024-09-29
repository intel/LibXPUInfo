@setlocal
set _EXTPATH=%~dp0

@REM %_EXTPATH%\NVML is a pre-created folder
@if EXIST %_EXTPATH%\NVML\lib\x64\nvml.lib if EXIST %_EXTPATH%\NVML\include\nvml.h goto NVML_DONE

@echo Make sure env var https_proxy is set if needed!
set NVML_BASE_NAME=cuda_nvml_dev-windows-x86_64-12.5.39-archive

if NOT EXIST NVML.zip curl.exe -o NVML.zip https://developer.download.nvidia.com/compute/cuda/redist/cuda_nvml_dev/windows-x86_64/%NVML_BASE_NAME%.zip
@if %ERRORLEVEL% neq 0 goto ERROR
c:\Windows\System32\tar.exe xvf NVML.zip
@if %ERRORLEVEL% neq 0 goto ERROR
if NOT EXIST %_EXTPATH%\NVML\lib move %NVML_BASE_NAME%\lib %_EXTPATH%\NVML
@if %ERRORLEVEL% neq 0 goto ERROR
if NOT EXIST %_EXTPATH%\NVML\include move %NVML_BASE_NAME%\include %_EXTPATH%\NVML
@if %ERRORLEVEL% neq 0 goto ERROR
rmdir /s /q %NVML_BASE_NAME%

:ERROR
exit /B %ERRORLEVEL%
:NVML_DONE

@endlocal
