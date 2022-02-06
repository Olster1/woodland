@echo off
cls

IF NOT EXIST ..\bin mkdir ..\bin
pushd ..\bin

set commonCompilerFlags=-nologo -FC -Zi -Gm- -GR- -EHa- -Zo -Oi -Zi -Od

cl /DDEBUG_BUILD=1 %commonCompilerFlags% ..\src\win32_main.cpp -FeWoodland /link user32.lib Comdlg32.lib d3d11.lib d3dcompiler.lib Shell32.lib
@echo Done

popd 