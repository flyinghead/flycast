PROJECT:=razdecy
LDFLAGS:=-lppapi_gles2 -lppapi_cpp -lppapi
SOURCES	:=	../src/cfg/ ../src/hw/arm7/ ../src/hw/aica/ ../src/hw/asic/ ../src/hw/ ../src/hw/gdrom/ ../src/hw/maple/ \
 ../src/hw/mem/ ../src/hw/pvr/ ../src/hw/sh4/ ../src/hw/sh4/rec_v2/ ../src/plugins/ ../src/profiler/ ../src/serial_ipc/ \
 ../src/hw/extdev/ ../src/hw/arm/ ../src/imgread/ \
 ../src/nacl/ ../src/ ../src/deps/zlib/ ../src/deps/chdr/ ../src/deps/crypto/ ../src/arm_emitter/ 


CXX_SOURCES := $(foreach dir,$(SOURCES),$(wildcard $(dir)*.cpp))

CC_SOURCES += $(foreach dir,$(SOURCES),$(wildcard $(dir)*.c))



# Project Build flags
WARNINGS:=-Wno-long-long -Wall -Wswitch-enum -pedantic
CXXFLAGS:=-pthread -std=gnu++98 $(WARNINGS)

CXXFLAGS	+= -I../src/ -I../src.khronos -I../linux-deps/include -D RELEASE -D HOST_NO_REC -D TARGET_NACL32

#
# Compute tool paths
#
#
OSNAME:=$(shell python $(NACL_SDK_ROOT)/tools/getos.py)
TC_PATH:=$(NACL_SDK_ROOT)/toolchain/$(OSNAME)_x86_newlib
CXX:=$(TC_PATH)/bin/i686-nacl-g++
CC:=$(TC_PATH)/bin/i686-nacl-gcc

#
# Disable DOS PATH warning when using Cygwin based tools Windows
#
CYGWIN ?= nodosfilewarning
export CYGWIN


# Declare the ALL target first, to make the 'all' target the default build
all: $(PROJECT)_x86_32.nexe

# Define 32 bit compile and link rules for C++ sources
CXX_32_OBJS:=$(patsubst %.cpp,%.nacl_86x32_build_obj,$(CXX_SOURCES))
#CC_32_OBJS:=$(patsubst %.c,%.build_obj_nacl_32,$(CC_SOURCES))

x86_32_OBJS:=$(CXX_32_OBJS) $(CC_32_OBJS)

%.nacl_86x32_build_obj : %.cpp $(THIS_MAKE)
	$(CXX) -o $@ -c $< -m32 -O0 -g $(CXXFLAGS)
	
%.nacl_86x32_build_obj : %.c $(THIS_MAKE)
	$(CC) -o $@ -c $< -m32 -O0 -g $(CXXFLAGS)

$(PROJECT)_x86_32.nexe : $(x86_32_OBJS)
	$(CXX) -o $@ $^ -m32 -O0 -g $(CXXFLAGS) $(LDFLAGS)

clean:
	rm $(x86_32_OBJS)

# Define a phony rule so it always runs, to build nexe and start up server.
.PHONY: RUN 
RUN: all
	python ../httpd.py


