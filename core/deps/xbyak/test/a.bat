@echo off
echo 32bit
rm -rf a.lst b.lst
echo nasm
nasm -l a.lst -f win32 -DWIN32 test.asm
cat a.lst
echo yasm
yasm -l b.lst -f win32 -DWIN32 test.asm
cat b.lst
