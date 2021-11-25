@echo off
where /q cl
if ERRORLEVEL 1 (
    call  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    call  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

set VK_PATH = C:/VulkanSDK/1.2.189.2
set application_name=game.exe
set build_options= -DBUILD_DEVELOPER=1 -DBUILD_DEBUG=1 -DBUILD_WIN32=1
set compile_flags= -nologo -FC /W0 /Zi /EHsc -I../src
set link_flags= -incremental:no -opt:ref user32.lib  ../src/ext/GLFW/glfw3.lib C:/VulkanSDK/1.2.189.2/Lib/vulkan-1.lib

REM The fully code is compatible with C++, to compile just remove /Tc from the cl.exe command

if not exist build mkdir build
pushd build
"cl.exe" %compile_flags% %build_options% /MD /Tc ..\src\vk_base.c -I../src/ -I../src/ext/GLFW/ -IC:/VulkanSDK/1.2.189.2/Include/ /link %link_flags% /out:%application_name%
popd