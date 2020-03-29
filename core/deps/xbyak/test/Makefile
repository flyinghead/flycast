TARGET = make_nm normalize_prefix jmp address bad_address misc cvt_test cvt_test32
XBYAK_INC=../xbyak/xbyak.h
BIT=32
ifeq ($(shell uname -m),x86_64)
BIT=64
endif

ifeq ($(BIT),64)
	TARGET += jmp64 address64
endif

all: $(TARGET)

CFLAGS_WARN=-Wall -Wextra -Wformat=2 -Wcast-qual -Wcast-align -Wwrite-strings -Wfloat-equal -Wpointer-arith

CFLAGS=-O2 -fomit-frame-pointer -Wall -fno-operator-names -I../ -I./ $(CFLAGS_WARN) #-std=c++0x
make_nm:
	$(CXX) $(CFLAGS) make_nm.cpp -o $@
normalize_prefix: normalize_prefix.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) normalize_prefix.cpp -o $@
test_mmx: test_mmx.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) test_mmx.cpp -o $@ -lpthread
jmp: jmp.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) jmp.cpp -o $@ -m32
jmp64: jmp.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) jmp.cpp -o $@ -m64
address: address.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) address.cpp -o $@ -m32
address64: address.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) address.cpp -o $@ -m64
bad_address: bad_address.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) bad_address.cpp -o $@
misc: misc.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) misc.cpp -o $@
cvt_test: cvt_test.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) $< -o $@
cvt_test32: cvt_test.cpp ../xbyak/xbyak.h
	$(CXX) $(CFLAGS) $< -o $@ -DXBYAK32

test_nm: normalize_prefix jmp bad_address $(TARGET)
	$(MAKE) -C ../gen
	./test_nm.sh
	./test_nm.sh Y
	./test_nm.sh avx512
	./test_address.sh
	./jmp
	./bad_address
	./misc
	./cvt_test
	./cvt_test32
ifeq ($(BIT),64)
	./test_address.sh 64
	./test_nm.sh 64
	./test_nm.sh Y64
	./jmp64
endif

test_avx: normalize_prefix
	./test_avx.sh
	./test_avx.sh Y
ifeq ($(BIT),64)
	./test_address.sh 64
	./test_avx.sh 64
	./test_avx.sh Y64
endif

test_avx512: normalize_prefix
	./test_avx512.sh
ifeq ($(BIT),64)
	./test_avx512.sh 64
endif

test:
	$(MAKE) test_nm
	$(MAKE) test_avx
	$(MAKE) test_avx512

clean:
	rm -rf *.o $(TARGET) lib_run nm.cpp nm_frame make_512

lib_run: lib_test.cpp lib_run.cpp lib.h
	$(CXX) $(CFLAGS) lib_run.cpp lib_test.cpp -o lib_run
make_nm: make_nm.cpp $(XBYAK_INC)


