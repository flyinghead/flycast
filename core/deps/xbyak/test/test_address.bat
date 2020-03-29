@echo off
set FILTER=grep -v warning
if /i "%1"=="64" (
	set OPT2=-DXBYAK64
	set OPT3=win64
) else (
	set OPT2=-DXBYAK32
	set OPT3=win32
)

call set_opt
bmake -f Makefile.win all

if /i "%1"=="64" (
	call :sub 1
	call :sub 2
) else (
	call :sub 1
)
goto end

:sub
echo cl address.cpp %OPT% %OPT2%
cl address.cpp %OPT% %OPT2%
address %1% > a.asm
echo nasm -f %OPT3% -l a.lst a.asm
nasm -f %OPT3% -l a.lst a.asm
awk "{if (index($3, ""-"")) { conti=substr($3, 0, length($3) - 1) } else { conti = conti $3; print conti; conti = """" }} " < a.lst |%FILTER% > ok.lst
echo address %1% jit > nm.cpp
address %1% jit > nm.cpp
echo cl -I../ -DXBYAK_TEST nm_frame.cpp %OPT% %OPT2%
cl -I../ -DXBYAK_TEST nm_frame.cpp %OPT% %OPT2%
nm_frame > x.lst
diff -w x.lst ok.lst
wc x.lst

:end
