call set_opt
bmake -f Makefile.win all
cl -I../ -I./ -DXBYAK_TEST misc.cpp %OPT% /Od /Zi
misc
