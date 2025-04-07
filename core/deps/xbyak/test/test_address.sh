#!/bin/sh

set -e

FILTER="grep -v warning"

sub()
{

CFLAGS="-Wall -I../ $OPT2"
CXX=${CXX:=g++}

echo "compile address.cpp"
$CXX $CFLAGS address.cpp -o address

./address $1 > a.asm
echo "asm"
$EXE -f$OPT3 a.asm -l a.lst
awk '{printf "%s", sub(/-$/, "", $3) ? $3 : $3 ORS}' a.lst | $FILTER > ok.lst

echo "xbyak"
./address $1 jit > nm.cpp
echo "compile nm_frame.cpp"
$CXX $CFLAGS -DXBYAK_TEST nm_frame.cpp -o nm_frame
./nm_frame > x.lst
diff -bB ok.lst x.lst && echo "ok"

}

if [ "$1" = "64" ]; then
	echo "nasm(64bit)"
	EXE=nasm
	OPT2=-DXBYAK64
	OPT3=win64

	sub 1
	sub 2
else
	echo "nasm(32bit)"
	EXE=nasm
	OPT2=-DXBYAK32
	OPT3=win32
	sub 1
fi

