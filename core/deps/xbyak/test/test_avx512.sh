#!/bin/tcsh

set FILTER="grep -v warning"

if ($1 == "64") then
	echo "nasm(64bit)"
	set EXE=nasm
	set OPT2=-DXBYAK64
	set OPT3=win64
	set FILTER=./normalize_prefix
else
	echo "nasm(32bit)"
	set EXE=nasm
	set OPT2=-DXBYAK32
	set OPT3=win32
endif

set CFLAGS="-Wall -fno-operator-names -I../ $OPT2 -DUSE_AVX512"
echo "compile make_512.cpp"
g++ $CFLAGS make_512.cpp -o make_512

./make_512 > a.asm
echo "asm"
$EXE -f$OPT3 a.asm -l a.lst
awk '{if (index($3, "-")) { conti=substr($3, 0, length($3) - 1) } else { conti = conti $3; print conti; conti = "" }} ' < a.lst | $FILTER > ok.lst

echo "xbyak"
./make_512 jit > nm.cpp
echo "compile nm_frame.cpp"
g++ $CFLAGS -DXBYAK_TEST nm_frame.cpp -o nm_frame -DXBYAK_AVX512
./nm_frame | $FILTER > x.lst
diff -B ok.lst x.lst && echo "ok"
