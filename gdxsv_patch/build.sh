#!/bin/bash
set -eux

SCRIPT_DIR=$(cd $(dirname "$0"); pwd)
cd "$SCRIPT_DIR"

rm -rf ./bin
mkdir bin

sh4-linux-gnu-gcc-9 -O2 -fno-stack-protector src/main.c -c -o bin/main.x
sh4-linux-gnu-ld -T src/ld-disk2.script bin/main.x -o bin/main.o
sh4-linux-gnu-objdump -h bin/main.o
sh4-linux-gnu-objcopy \
    --only-section gdx.inject --only-section gdx.main \
    --only-section gdx.data --only-section gdx.func \
    bin/main.o bin/gdxpatch.o
sh4-linux-gnu-objdump -h -D bin/gdxpatch.o > bin/gdxpatch.asm
