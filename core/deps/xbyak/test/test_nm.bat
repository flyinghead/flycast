@echo off
set FILTER=cat
set Y=0
if /i "%1"=="Y" (
	set Y=1
	set EXE=yasm.exe
	set OPT2=-DUSE_YASM -DXBYAK32
	set OPT3=win32
) else if /i "%1"=="64" (
	set EXE=nasm.exe
	set OPT2=-DXBYAK64
	set OPT3=win64
	set FILTER=normalize_prefix
) else if /i "%1"=="Y64" (
	set Y=1
	set EXE=yasm.exe
	set OPT2=-DUSE_YASM -DXBYAK64
	set OPT3=win64
	set FILTER=normalize_prefix
) else if /i "%1"=="noexcept" (
	set EXE=nasm.exe
	set OPT2=-DXBYAK32 -DXBYAK_NO_EXCEPTION
	set OPT3=win32
) else (
	set EXE=nasm.exe
	set OPT2=-DXBYAK32
	set OPT3=win32
)
call set_opt
bmake -f Makefile.win all
echo cl -I../ make_nm.cpp %OPT% %OPT2% /EHs
cl -I../ make_nm.cpp %OPT% %OPT2% /EHs
make_nm > a.asm
rm -rf a.lst
echo %EXE% -f %OPT3% -l a.lst a.asm
%EXE% -f %OPT3% -l a.lst a.asm
rem connect "?????-" and "??"
if /i "%Y%"=="1" (
	awk "NR > 1 {if (index($3, ""-"")) { conti=substr($3, 0, length($3) - 1) } else { conti = conti $3; print conti; conti = """" }} " < a.lst |%FILTER% > ok.lst
) else (
	awk "{if (index($3, ""-"")) { conti=substr($3, 0, length($3) - 1) } else { conti = conti $3; print conti; conti = """" }} " < a.lst |%FILTER% > ok.lst
)
make_nm jit > nm.cpp
cl -I../ -DXBYAK_TEST nm_frame.cpp %OPT% %OPT2%
nm_frame |%FILTER% > x.lst
diff -wb x.lst ok.lst && echo "ok"
