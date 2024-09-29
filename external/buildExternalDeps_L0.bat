@setlocal

@set _CMAKEGEN="Visual Studio 17 2022"
set _EXTPATH=%~dp0

@where cmake
@if %ERRORLEVEL% neq 0 (
    echo cmake not found!
    goto ERROR
)

cmake -S %_EXTPATH%/level-zero -B %_EXTPATH%/level-zero-build -D CMAKE_INSTALL_PREFIX=%_EXTPATH%/level-zero-build/install -G %_CMAKEGEN%  -A x64
if %ERRORLEVEL% neq 0 goto ERROR
cmake --build %_EXTPATH%/level-zero-build --target install --config RelWithDebInfo
if %ERRORLEVEL% neq 0 goto ERROR

goto END
:ERROR
@echo FAILED!
@exit /b -1

:END
@endlocal
