@echo off
set GLSLANG_PATH = "C:/VulkanSDK/1.2.189.2/Bin/"
set SHADER_PATH = "..\assets\"

pushd build
mkdir shaders
popd
pushd assets\\shaders
FOR %%f IN (*) DO %GLSLANGPATH% "glslangvalidator.exe" -V %%f -o ../../build/shaders/%%f.spv
popd
