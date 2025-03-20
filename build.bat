@echo off

pushd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-mingw-static -G "Ninja"
ninja
popd

if %ERRORLEVEL% EQU 0 echo build success && .\cmake-build-release\ami.exe
