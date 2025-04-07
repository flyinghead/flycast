call set_opt
bmake -f Makefile.win all
cl -I../ -I./ -DXBYAK_TEST %FILE%.cpp %OPT% /Od /Zi
%FILE%
