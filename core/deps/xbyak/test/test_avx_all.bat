@echo off
echo ** nasm-avx(32bit) ***
call test_avx
echo ** nasm-avx(64bit) ***
call test_avx 64
echo ** yasm-avx(32bit) ***
call test_avx Y
echo ** yasm-avx(64bit) ***
call test_avx Y64
echo ** nasm-avx512(32bit) ***
call test_avx512
echo ** nasm-avx512(64bit) ***
call test_avx512 64
