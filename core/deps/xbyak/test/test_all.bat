@echo off
call test_nm_all
echo *** test addressing ***
call test_address
call test_address 64
echo *** test jmp address ***
call test_jmp
echo *** all test end ***
