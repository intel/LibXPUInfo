@setlocal
set _EXTPATH=%~dp0

@REM %_EXTPATH%\AGS_SDK is a pre-created folder
@if EXIST %_EXTPATH%\AGS_SDK\ags_lib\lib\amd_ags_x64_2022_MD.lib if EXIST %_EXTPATH%\AGS_SDK\ags_lib\inc\amd_ags.h goto AGS_SDK_DONE

@echo Make sure env var https_proxy is set if needed!
set AGS_SDK_BASE_NAME=https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK/archive/refs/tags
set AGS_SDK_VER=6.3.0

if NOT EXIST AGS_SDK.zip curl.exe -L -o AGS_SDK.zip %AGS_SDK_BASE_NAME%/v%AGS_SDK_VER%.zip
@if %ERRORLEVEL% neq 0 goto ERROR
c:\Windows\System32\tar.exe xvf AGS_SDK.zip
ren AGS_SDK-%AGS_SDK_VER% AGS_SDK
@if %ERRORLEVEL% neq 0 goto ERROR
del AGS_SDK.zip

:ERROR
exit /B %ERRORLEVEL%
:AGS_SDK_DONE
:END

@endlocal
