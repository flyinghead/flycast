call set_opt
bmake -f Makefile.win all
cl -I../ -I./ -DXBYAK_TEST jmp.cpp %OPT% /Od /Zi
jmp
