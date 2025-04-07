XBYAK_INC=../xbyak/xbyak.h
CXX?=g++

BOOST_EXIST=$(shell echo "#include <boost/spirit/core.hpp>" | $(CXX) -x c++ -c - 2>/dev/null && echo 1)
UNAME_M=$(shell uname -m)

ONLY_64BIT=0
ifeq ($(shell uname -s),Darwin)
	ONLY_64BIT=1
	OS=mac
	ifeq ($(UNAME_M),x86_64)
		BIT=64
	endif
	ifeq ($(UNAME_M),i386)
		BIT=32
	endif
	ifeq ($(shell sw_vers -productVersion | cut -c1-4 | sed 's/\.//'),105)
		ifeq ($(shell sysctl -n hw.cpu64bit_capable),1)
			BIT=64
		endif
	endif
else
	BIT=32
	ifeq ($(UNAME_M),x86_64)
		BIT=64
	endif
	ifeq ($(UNAME_M),amd64)
		BIT=64
	endif
endif

ifeq ($(BIT),64)
TARGET += test64 bf64 memfunc64 test_util64 jmp_table64 zero_upper ccmp no_flags
ifeq ($(BOOST_EXIST),1)
TARGET += calc64 #calc2_64
endif
endif

ifneq ($(OS),mac)
TARGET += static_buf64
TARGET += memfd
endif


ifneq ($(ONLY_64BIT),1)
	TARGET += test quantize bf toyvm test_util memfunc static_buf jmp_table
ifeq ($(BOOST_EXIST),1)
	TARGET += calc #calc2
endif
endif

all: $(TARGET)

CFLAGS_WARN=-Wall -Wextra -Wformat=2 -Wcast-qual -Wcast-align -Wwrite-strings -Wfloat-equal -Wpointer-arith #-pedantic

CFLAGS=-g -O2 -fomit-frame-pointer -Wall -I../ $(CFLAGS_WARN) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS)

test:
	$(CXX) $(CFLAGS) test0.cpp -o $@ -m32

quantize:
	$(CXX) $(CFLAGS) quantize.cpp -o $@ -m32

calc:
	$(CXX) $(CFLAGS) calc.cpp -o $@ -m32
calc64:
	$(CXX) $(CFLAGS) calc.cpp -o $@ -m64
calc2:
	$(CXX) $(CFLAGS) calc2.cpp -o $@ -m32
calc2_64:
	$(CXX) $(CFLAGS) calc2.cpp -o $@ -m64

bf:
	$(CXX) $(CFLAGS) bf.cpp -o $@ -m32
bf64:
	$(CXX) $(CFLAGS) bf.cpp -o $@ -m64

memfunc:
	$(CXX) $(CFLAGS) memfunc.cpp -o $@ -m32
memfunc64:
	$(CXX) $(CFLAGS) memfunc.cpp -o $@ -m64

toyvm:
	$(CXX) $(CFLAGS) toyvm.cpp -o $@ -m32

test64:
	$(CXX) $(CFLAGS) test0.cpp -o $@ -m64
test_util:
	$(CXX) $(CFLAGS) test_util.cpp -o $@ -m32
test_util64:
	$(CXX) $(CFLAGS) test_util.cpp -o $@ -m64
static_buf:
	$(CXX) $(CFLAGS) static_buf.cpp -o $@ -m32
static_buf64:
	$(CXX) $(CFLAGS) static_buf.cpp -o $@ -m64
jmp_table:
	$(CXX) $(CFLAGS) jmp_table.cpp -o $@ -m32
jmp_table64:
	$(CXX) $(CFLAGS) jmp_table.cpp -o $@ -m64
memfd:
	$(CXX) $(CFLAGS) memfd.cpp -o $@ -m64
profiler: profiler.cpp ../xbyak/xbyak_util.h
	$(CXX) $(CFLAGS) profiler.cpp -o $@
profiler-vtune: profiler.cpp ../xbyak/xbyak_util.h
	$(CXX) $(CFLAGS) profiler.cpp -o $@ -DXBYAK_USE_VTUNE -I /opt/intel/vtune_amplifier/include/ -L /opt/intel/vtune_amplifier/lib64 -ljitprofiling -ldl
zero_upper: zero_upper.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) zero_upper.cpp -o $@
test_zero_upper: zero_upper
	sde -future -- ./zero_upper
ccmp: ccmp.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) ccmp.cpp -o $@
test_ccmp: ccmp
	sde -future -- ./ccmp
no_flags: no_flags.cpp $(XBYAK_INC)
	$(CXX) $(CFLAGS) no_flags.cpp -o $@
test_no_flags: no_flags
	sde -future -- ./no_flags

clean:
	rm -rf $(TARGET) profiler profiler-vtune

test : test0.cpp $(XBYAK_INC)
test64: test0.cpp $(XBYAK_INC)
quantize : quantize.cpp $(XBYAK_INC)
calc : calc.cpp $(XBYAK_INC)
calc64 : calc.cpp $(XBYAK_INC)
calc2 : calc2.cpp $(XBYAK_INC)
calc2_64 : calc2.cpp $(XBYAK_INC)
bf : bf.cpp $(XBYAK_INC)
bf64 : bf.cpp $(XBYAK_INC)
memfunc : memfunc.cpp $(XBYAK_INC)
memfunc64 : memfunc.cpp $(XBYAK_INC)
toyvm : toyvm.cpp $(XBYAK_INC)
static_buf: static_buf.cpp $(XBYAK_INC)
static_buf64: static_buf.cpp $(XBYAK_INC)
test_util : test_util.cpp $(XBYAK_INC) ../xbyak/xbyak_util.h
test_util64 : test_util.cpp $(XBYAK_INC) ../xbyak/xbyak_util.h
jmp_table: jmp_table.cpp $(XBYAK_INC)
jmp_table64: jmp_table.cpp $(XBYAK_INC)
memfd: memfd.cpp $(XBYAK_INC)
