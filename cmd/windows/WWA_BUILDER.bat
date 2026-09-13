@echo off
setlocal
cd /d "%~dp0..\.."
if not exist "bin_exe\windows\release\wwa_build.exe" (
  if not exist "bin_obj\windows\release\obj" mkdir "bin_obj\windows\release\obj"
  if not exist "bin_exe\windows\release" mkdir "bin_exe\windows\release"
  gcc -O3 -std=c11 -Wall -Wextra -fno-builtin -fno-stack-protector -Isrc/std ^
      src\utility\builder\wwa_build.c ^
      src\std\stdos.c src\std\stdtime.c src\std\stdmem.c src\std\stdmem_avx2.c src\std\stdstr.c src\std\stdstr_avx2.c src\std\stdmath.c src\std\stdfloat.c src\std\stdio.c src\std\stdlib.c src\std\stdlog.c src\std\wwa_crt.c ^
      -mavx2 -nostdlib -lkernel32 -Wl,-e,mainCRTStartup -o bin_exe\windows\release\wwa_build.exe
)
if errorlevel 1 exit /b 1
bin_exe\windows\release\wwa_build.exe %*
endlocal