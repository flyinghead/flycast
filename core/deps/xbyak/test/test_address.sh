#!/bin/sh

FILTER="grep -v warning"

sub()
{

CFLAGS="-Wall -fno-operator-names -I../ $OPT2"
echo "compile address.cpp"
g++ $CFLAGS address.cpp -o address

./address $1 > a.asm
echo "asm"
$EXE -f$OPT3 a.asm -l a.lst
awk '{if (index($3, "-")) { conti=substr($3, 0, length($3) - 1) } else { conti = conti $3; print conti; conti = "" }} ' < a.lst | $FILTER > ok.lst

echo "xbyak"
./address $1 jit > nm.cpp
echo "compile nm_frame.cpp"
g++ $CFLAGS -DXBYAK_TEST nm_frame.cpp -o nm_frame
./nm_frame > x.lst
diff ok.lst x.lst && echo "ok"

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

