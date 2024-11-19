@echo off

set WarningEliminations=-wd4100 -wd4201 -wd4820 -wd4191 -wd5045
set CompilerFlags=-WL -nologo -Gm- -WX -Wall %WarningEliminations% -Od -Oi -Z7 -DDEBUG 
set OutputPath=-Fe"build/bin/CBluetooth" -Fo"build/"
set Libraries=vulkan-1.lib user32.lib Winmm.lib
set Files=./c/main.c
set IncludeDirs=-I./c 
set LibraryDirs=
REM set LibraryDirs=/LIBPATH:"C:/VulkanSDK/1.3.283.0/Lib" /LIBPATH:./build

IF NOT EXIST build/bin mkdir build\bin

cl %CompilerFlags% %OutputPath% %IncludeDirs% %Files% /link /SUBSYSTEM:console %StbLibs% %LibraryDirs% %Libraries%

IF NOT %errorlevel% neq 0 (echo Build Completed Successfully) ELSE (echo ERROR Build Stopped)
set HHHHHHH=1
