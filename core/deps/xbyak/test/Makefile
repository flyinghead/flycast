# To compile with -m32
# apt install g++-multilib
TARGET = make_nm normalize_prefix bad_address misc cvt_test cvt_test32 noexception misc32 detect_x32 avx10_test
XBYAK_INC=../xbyak/xbyak.h ../xbyak/xbyak_mnemonic.h ../xbyak/xbyak_util.h
UNAME_S=$(shell uname -s)
ifeq ($(shell ./detect_x32),x32)
X32?=1
endif
BIT=32
ifeq ($(shell uname -m),x86_64)
BIT=64
endif
ONLY_64BIT=0
ifeq ($(UNAME_S),Darwin)
  # 32-bit binary is not supported
  ONLY_64BIT=1
endif
ifeq ($(findstring MINGW64,$(UNAME_S)),MINGW64)
  ONLY_64BIT=1
endif
ifeq ($(ONLY_64BIT),0)
  TARGET += jmp address
endif

ifeq ($(BIT),64)
	TARGET += jmp64 address64 apx
	TARGET += sf_test
endif

all: $(TARGET)

CFLAGS_WARN=-Wall -Wextra -Wformat=2 -Wcast-qual -Wwrite-strings -Wfloat-equal -Wpointer-arith

CFLAGS=-O2 -Wall -I.. -I. $(CFLAGS_WARN) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) #-std=c++0x
make_nm:
	$(CXX) $(CFLAGS) make_nm.cpp -o $@
normalize_prefix: normalize_prefix.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) normalize_prefix.cpp -o $@
test_mmx: test_mmx.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) test_mmx.cpp -o $@ -lpthread
jmp: jmp.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -m32
jmp64: jmp.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -m64
address: address.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -m32
address64: address.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -m64
bad_address: bad_address.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@
misc: misc.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@
misc32: misc.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -DXBYAK32
cvt_test: cvt_test.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@
cvt_test32: cvt_test.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -DXBYAK32
noexception: noexception.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -fno-exceptions
apx: apx.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@
avx10_test: avx10_test.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -DXBYAK64
sf_test: sf_test.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) $< -o $@ -DXBYAK64

TEST_FILES=old.txt new-ymm.txt bf16.txt comp.txt misc.txt convert.txt minmax.txt saturation.txt
xed_test:
	@set -e; \
	for target in $(addprefix avx10/, $(TEST_FILES)); do \
		./test_by_xed.sh $$target || exit 1; \
	done

test_nm: normalize_prefix $(TARGET)
	$(MAKE) -C ../gen
ifneq ($(ONLY_64BIT),1)
	CXX=$(CXX) ./test_nm.sh
	CXX=$(CXX) ./test_nm.sh noexcept
	CXX=$(CXX) ./test_nm.sh Y
	CXX=$(CXX) ./test_nm.sh avx512
	CXX=$(CXX) ./test_address.sh
	./jmp
	./cvt_test32
endif
	./bad_address
	./misc
	./misc32
	./cvt_test
ifeq ($(BIT),64)
	CXX=$(CXX) ./test_address.sh 64
ifneq ($(X32),1)
	CXX=$(CXX) ./test_nm.sh 64
	CXX=$(CXX) ./test_nm.sh Y64
endif
	./jmp64
	./apx
	./avx10_test
endif

test_avx: normalize_prefix
ifneq ($(ONLY_64BIT),0)
	CXX=$(CXX) ./test_avx.sh
	CXX=$(CXX) ./test_avx.sh Y
endif
ifeq ($(BIT),64)
	CXX=$(CXX) ./test_avx.sh 64
ifneq ($(X32),1)
	CXX=$(CXX) ./test_avx.sh Y64
endif
endif

test_avx512: normalize_prefix
ifneq ($(ONLY_64BIT),0)
	CXX=$(CXX) ./test_avx512.sh
endif
ifeq ($(BIT),64)
	CXX=$(CXX) ./test_avx512.sh 64
endif

test_avx10: avx10_test
	./avx10_test

detect_x32: detect_x32.c
	$(CC) $< -o $@

test: detect_x32
	$(MAKE) test_nm
	$(MAKE) test_avx
	$(MAKE) test_avx512

clean:
	$(RM) a.asm *.lst *.obj *.o $(TARGET) lib_run nm.cpp nm_frame make_512 avx10_test detect_x32

lib_run: lib_test.cpp lib_run.cpp lib.h
	$(CXX) $(CFLAGS) lib_run.cpp lib_test.cpp -o lib_run
make_nm: make_nm.cpp $(XBYAK_INC)

.PHONY: test
