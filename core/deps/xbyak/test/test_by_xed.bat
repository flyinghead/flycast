@echo off
set CFLAGS=-I ../ /EHsc /nologo
copy %1% tmp.cpp
cl %CFLAGS% test_by_xed.cpp && test_by_xed.exe
%XED% -64 -ir bin > out.txt
python3 test_by_xed.py %1% out.txt
