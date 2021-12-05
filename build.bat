@echo off
where /q cl
if ERRORLEVEL 1 (
    call  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
    call  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)

set VK_PATH = C:/VulkanSDK/1.2.189.2
set application_name=game.exe
set build_options= -DBUILD_DEVELOPER=1 -DBUILD_DEBUG=1 -DBUILD_WIN32=1
set compile_flags= -nologo -FC /W0 /Zi /EHsc -I../src -I../src/ -I../ext/
set link_flags= -incremental:no -opt:ref user32.lib  ../ext/GLFW/glfw3.lib C:/VulkanSDK/1.2.189.2/Lib/vulkan-1.lib C:/VulkanSDK/1.2.189.2/Lib/*.lib 

set precompiled_shaders = 1



if not exist build mkdir build

pushd build
REM if not exist spv.lib cl /EHsc  /LD ../ext/SPIRV/*.c
REM if not exist spv.lib lib -out:spv.lib *.obj 
cl %compile_flags% %build_options% /MT /Tc ..\ext\SPIRV\spirv_reflect.c /Tc ..\src\vk_base.c -I/../src/ext/SPIRV/*.h /link %link_flags% /out:%application_name%
popd

if not exist build\shaders CALL precompile_shaders