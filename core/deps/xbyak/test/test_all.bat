@echo off
call test_nm_all
echo *** test addressing ***
call test_address
call test_address 64
echo *** test jmp address ***
call test_jmp
echo *** test misc ***
set FILE=misc
call test_misc
echo *** test APX ***
set FILE=apx
call test_misc
echo *** test AVX10 ***
set FILE=avx10_test
call test_misc
echo *** all test end ***
