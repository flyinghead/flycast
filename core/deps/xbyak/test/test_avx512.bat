@echo off
set FILTER=cat
set Y=0
if /i "%1"=="min" (
	set EXE=nasm.exe
	set OPT2=-DXBYAK64 -DMIN_TEST
	set OPT3=win64
	set FILTER=normalize_prefix
) else if /i "%1"=="64" (
	set EXE=nasm.exe
	set OPT2=-DXBYAK64
	set OPT3=win64
	set FILTER=normalize_prefix
) else (
	set EXE=nasm.exe
	set OPT2=-DXBYAK32
	set OPT3=win32
)
call set_opt
bmake -f Makefile.win all
echo cl -I../ make_512.cpp %OPT% %OPT2% /EHs /DUSE_AVX512
cl -I../ make_512.cpp %OPT% %OPT2% /EHs /DUSE_AVX512
make_512 > a.asm
%EXE% -f %OPT3% -l a.lst a.asm
rem connect "?????-" and "??"
awk "{if (index($3, ""-"")) { conti=substr($3, 0, length($3) - 1) } else { conti = conti $3; print conti; conti = """" }} " < a.lst |%FILTER% > ok.lst
make_512 jit > nm.cpp
cl -I../ -DXBYAK_TEST nm_frame.cpp %OPT% %OPT2% /DXBYAK_AVX512
nm_frame |%FILTER% > x.lst
diff -w x.lst ok.lst
wc x.lst
