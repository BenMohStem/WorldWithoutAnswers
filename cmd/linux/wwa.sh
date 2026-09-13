#!/bin/sh
# World Without Answers build launcher (linux)
set -e
cd "$(dirname "$0")/../.."
if [ ! -f bin_exe/linux/release/wwa_build ]; then
  mkdir -p bin_obj/linux/release/obj bin_exe/linux/release
  gcc -O3 -std=c11 -Wall -Wextra -fno-builtin -fno-stack-protector -Isrc/std \
      src/utility/builder/wwa_build.c \
      src/std/stdos.c src/std/stdtime.c src/std/stdmem.c src/std/stdmem_avx2.c src/std/stdstr.c src/std/stdstr_avx2.c src/std/stdmath.c src/std/stdfloat.c src/std/stdio.c src/std/stdlib.c src/std/stdlog.c src/std/wwa_crt.c \
      -mavx2 -nostdlib -static -o bin_exe/linux/release/wwa_build
fi
exec bin_exe/linux/release/wwa_build "$@"