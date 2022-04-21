@echo off

del assets\shaders\*.spv

for /r %%v in (assets\shaders\*.vert,assets\shaders\*.frag) do (
    %VULKAN_SDK%\Bin\glslc.exe %%v -o %%v.spv
    echo compiled %%~nxv to %%~nxv.spv
)
