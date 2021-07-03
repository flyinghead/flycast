#!/bin/bash
set -eux

# sudo apt install g++-9-sh4-linux-gnu

# sh4-linux-gnu-addr2line     sh4-linux-gnu-gcc-nm-9      sh4-linux-gnu-objcopy
# sh4-linux-gnu-ar            sh4-linux-gnu-gcc-ranlib-9  sh4-linux-gnu-objdump
# sh4-linux-gnu-as            sh4-linux-gnu-gcov-9        sh4-linux-gnu-ranlib
# sh4-linux-gnu-c++filt       sh4-linux-gnu-gcov-dump-9   sh4-linux-gnu-readelf
# sh4-linux-gnu-cpp-9         sh4-linux-gnu-gcov-tool-9   sh4-linux-gnu-size
# sh4-linux-gnu-elfedit       sh4-linux-gnu-gprof         sh4-linux-gnu-strings
# sh4-linux-gnu-g++-9         sh4-linux-gnu-ld            sh4-linux-gnu-strip
# sh4-linux-gnu-gcc-9         sh4-linux-gnu-ld.bfd
# sh4-linux-gnu-gcc-ar-9      sh4-linux-gnu-nm

SCRIPT_DIR=$(cd $(dirname "$0"); pwd)
cd "$SCRIPT_DIR"

rm -rf ./bin
mkdir bin

sh4-linux-gnu-gcc-9 -O2 -fno-stack-protector src/main.c -c -o bin/main.x
sh4-linux-gnu-ld -T src/ld.script bin/main.x -o bin/main.o
sh4-linux-gnu-objdump -h bin/main.o
sh4-linux-gnu-objcopy \
    --only-section gdx.main1 --only-section gdx.main2 \
    --only-section gdx.data --only-section gdx.func \
    bin/main.o bin/gdxsv_patch.o
sh4-linux-gnu-objdump -h -D bin/gdxsv_patch.o > bin/gdxsv_patch.asm

sh4-linux-gnu-gcc-9 -O2 -fno-stack-protector src/cheat.c -c -o bin/cheat.x
sh4-linux-gnu-ld -T src/ld.script bin/cheat.x -o bin/cheat.o
sh4-linux-gnu-objdump -h bin/cheat.o
sh4-linux-gnu-objcopy --only-section gdx.cheat bin/cheat.o bin/gdxsv_cheat.o
sh4-linux-gnu-objdump -h -D bin/gdxsv_cheat.o > bin/gdxsv_cheat.asm
