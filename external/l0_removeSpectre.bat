set _EXTPATH=%~dp0

pushd %_EXTPATH%\level-zero
git apply %_EXTPATH%\l0_CMakeLists.patch
popd
