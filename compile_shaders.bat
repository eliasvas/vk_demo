@echo off
pushd build
"C:/VulkanSDK/1.2.189.2/Bin/glslc.exe" ../assets/shader.vert -o vert.spv
"C:/VulkanSDK/1.2.189.2/Bin/glslc.exe" ../assets/shader.frag -o frag.spv
popd