# Copilot Instructions

## Project Guidelines
- In LibXPUInfo project, optional vendor SDK dependencies (NVML, AGS, CUDA, etc.) are fetched via external\\get*Dep.bat scripts, gated behind XPUINFO_USE_* preprocessor defines, and delay-loaded at runtime (using delayimp.h + SEH-guarded init functions) rather than requiring hard DLL dependencies.