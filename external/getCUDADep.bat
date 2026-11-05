@setlocal
set _EXTPATH=%~dp0

@REM %_EXTPATH%\CUDA is a pre-created folder containing just the CUDA Driver API header + import lib.
@REM This avoids requiring a full CUDA Toolkit install - cuda.h and cuda.lib are lightweight
@REM and sufficient to query device ordinal/PCI info via the Driver API (nvcuda.dll, which
@REM ships with the NVIDIA display driver itself).
@if EXIST %_EXTPATH%\CUDA\lib\x64\cuda.lib if EXIST %_EXTPATH%\CUDA\include\cuda.h goto CUDA_DONE

@echo Make sure env var https_proxy is set if needed!
@REM NOTE: cuda.h (Driver API) and cuda.lib (stub import lib) are published as part of the
@REM "cuda_cudart" redistributable component. If NVIDIA reorganizes/renames this component,
@REM update CUDA_BASE_NAME and the URL below accordingly.
set CUDA_BASE_NAME=cuda_cudart-windows-x86_64-12.8.90-archive

if NOT EXIST CUDART.zip curl.exe -o CUDART.zip https://developer.download.nvidia.com/compute/cuda/redist/cuda_cudart/windows-x86_64/%CUDA_BASE_NAME%.zip
@if %ERRORLEVEL% neq 0 goto ERROR
c:\Windows\System32\tar.exe xvf CUDART.zip
@if %ERRORLEVEL% neq 0 goto ERROR
if NOT EXIST %_EXTPATH%\CUDA\include mkdir %_EXTPATH%\CUDA\include
@if %ERRORLEVEL% neq 0 goto ERROR
copy %CUDA_BASE_NAME%\include\cuda.h %_EXTPATH%\CUDA\include\cuda.h
@if %ERRORLEVEL% neq 0 goto ERROR
if NOT EXIST %_EXTPATH%\CUDA\lib\x64 mkdir %_EXTPATH%\CUDA\lib\x64
@if %ERRORLEVEL% neq 0 goto ERROR
copy %CUDA_BASE_NAME%\lib\x64\cuda.lib %_EXTPATH%\CUDA\lib\x64\cuda.lib
@if %ERRORLEVEL% neq 0 goto ERROR
rmdir /s /q %CUDA_BASE_NAME%
del CUDART.zip

:ERROR
exit /B %ERRORLEVEL%
:CUDA_DONE

@endlocal
