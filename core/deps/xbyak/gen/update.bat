@echo off
set OPT=/EHsc -I../ /W4 -D_CRT_SECURE_NO_WARNINGS
set TARGET=..\\xbyak\\xbyak_mnemonic.h
set SORT=sortline
cl gen_code.cpp %OPT%
gen_code | %SORT% > %TARGET%
echo #ifdef XBYAK_ENABLE_OMITTED_OPERAND>> %TARGET%
gen_code omit | %SORT% >> %TARGET%
echo #endif>>%TARGET%
gen_code fixed >> %TARGET%
cl gen_avx512.cpp %OPT%
echo #ifndef XBYAK_DISABLE_AVX512>> %TARGET%
gen_avx512 | %SORT% >> %TARGET%
echo #ifdef XBYAK64>> %TARGET%
gen_avx512 64 | %SORT% >> %TARGET%
echo #endif>> %TARGET%
echo #endif>> %TARGET%
