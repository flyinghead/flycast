#!/bin/sh

FILTER="grep -v warning"

case $1 in
Y)
	echo "yasm(32bit)"
	EXE=yasm
	OPT2="-DUSE_YASM -DXBYAK32"
	OPT3=win32
	;;
64)
	echo "nasm(64bit)"
	EXE=nasm
	OPT2=-DXBYAK64
	OPT3=win64
	FILTER=./normalize_prefix
	;;
Y64)
	echo "yasm(64bit)"
	EXE=yasm
	OPT2="-DUSE_YASM -DXBYAK64"
	OPT3=win64
	FILTER=./normalize_prefix
	;;
*)
	echo "nasm(32bit)"
	EXE=nasm
	OPT2=-DXBYAK32
	OPT3=win32
	;;
esac

CFLAGS="-Wall -fno-operator-names -I../ $OPT2 -DUSE_AVX"
echo "compile make_nm.cpp"
g++ $CFLAGS make_nm.cpp -o make_nm

./make_nm > a.asm
echo "asm"
$EXE -f$OPT3 a.asm -l a.lst
awk '$3 != "1+1" {printf "%s", sub(/-$/, "", $3) ? $3 : $3 ORS}' a.lst | $FILTER > ok.lst

echo "xbyak"
./make_nm jit > nm.cpp
echo "compile nm_frame.cpp"
g++ $CFLAGS -DXBYAK_TEST nm_frame.cpp -o nm_frame
./nm_frame | $FILTER > x.lst
diff -B ok.lst x.lst && echo "ok"
