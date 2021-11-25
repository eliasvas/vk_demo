@echo off

set application_name=app.exe
set build_options= -DBUILD_DEVELOPER=1 -DBUILD_DEBUG=1 -DBUILD_WIN32=1
set compile_flags= -nologo -FC /W0 /Zi /EHsc  -I../src

REM The fully code is compatible with C++, to compile just remove /Tc from the cl.exe command

if not exist build mkdir build
pushd build
"../tcc/tcc" -Wall -Wl,-subsystem=windows -m64 -IC:/VulkanSDK/1.2.189.2/Include/ -LC:/VulkanSDK/1.2.189.2/Lib/ -lvulkan-1 -luser32 -lgdi32  ../src/vk_base.c -o app.exe
popd