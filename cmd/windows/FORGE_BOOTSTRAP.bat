@echo off
rem World Without Answers - forge bootstrap (Windows)
rem Compiles the forge build engine directly with gcc once; afterwards forge
rem builds itself and the whole project (forge build).
setlocal
cd /d "%~dp0..\.."
if not exist "bin_obj\windows\release\obj" mkdir "bin_obj\windows\release\obj"
if not exist "bin_exe\windows\release" mkdir "bin_exe\windows\release"

set INC=-Isrc/std -Isrc/forge -Isrc/engine/physics -Isrc/engine/render -Isrc/engine/audio -Isrc/engine/input -Isrc/engine/window
set STD=src\std\stdos.c src\std\stdtime.c src\std\stdmem.c src\std\stdmem_avx2.c src\std\stdstr.c src\std\stdstr_avx2.c src\std\stdhash.c src\std\stdthread.c src\std\stdmath.c src\std\stdfloat.c src\std\stdio.c src\std\stdlib.c src\std\stdlog.c src\std\wwa_crt.c
set FORGE=src\forge\forge_main.c src\forge\forge_graph.c src\forge\forge_manifest.c src\forge\forge_sched.c

gcc -O2 -std=c11 -fno-builtin -fno-stack-protector %INC% %FORGE% %STD% -mavx2 -nostdlib -lkernel32 -luser32 -lgdi32 -Wl,-e,mainCRTStartup -o bin_exe\windows\release\forge.exe
if errorlevel 1 exit /b 1
echo bootstrap ok: bin_exe\windows\release\forge.exe
endlocal
