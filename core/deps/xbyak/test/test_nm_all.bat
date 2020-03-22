@echo off
echo *** nasm(32bit) ***
call test_nm
echo *** yasm(32bit) ***
call test_nm Y
echo *** nasm(64bit) ***
call test_nm 64
echo *** yasm(64bit) ***
call test_nm Y64

call test_avx_all
