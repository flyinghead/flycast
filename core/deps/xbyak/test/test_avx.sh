#!/bin/tcsh

set FILTER="grep -v warning"

if ($1 == "Y") then
	echo "yasm(32bit)"
	set EXE=yasm
	set OPT2="-DUSE_YASM -DXBYAK32"
	set OPT3=win32
else if ($1 == "64") then
	echo "nasm(64bit)"
	set EXE=nasm
	set OPT2=-DXBYAK64
	set OPT3=win64
	set FILTER=./normalize_prefix
else if ($1 == "Y64") then
	echo "yasm(64bit)"
	set EXE=yasm
	set OPT2="-DUSE_YASM -DXBYAK64"
	set OPT3=win64
	set FILTER=./normalize_prefix
else
	echo "nasm(32bit)"
	set EXE=nasm
	set OPT2=-DXBYAK32
	set OPT3=win32
endif

set CFLAGS="-Wall -fno-operator-names -I../ $OPT2 -DUSE_AVX"
echo "compile make_nm.cpp"
g++ $CFLAGS make_nm.cpp -o make_nm

./make_nm > a.asm
echo "asm"
$EXE -f$OPT3 a.asm -l a.lst
awk '{if (index($3, "-")) { conti=substr($3, 0, length($3) - 1) } else { conti = conti $3; print conti; conti = "" }} ' < a.lst | $FILTER | grep -v "1+1" > ok.lst

echo "xbyak"
./make_nm jit > nm.cpp
echo "compile nm_frame.cpp"
g++ $CFLAGS -DXBYAK_TEST nm_frame.cpp -o nm_frame
./nm_frame | $FILTER > x.lst
diff -B ok.lst x.lst && echo "ok"
