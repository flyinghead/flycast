
LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
NOT_ARM := 1
NO_REC := 1
#NO_REND := 1

RZDCY_SRC_DIR = ../../core

include $(RZDCY_SRC_DIR)/core.mk

PROJECT:=reicast
LDFLAGS:=-lppapi_gles2 -lppapi_cpp -lppapi


#CXX_SOURCES := $(foreach dir,$(SOURCES),$(wildcard $(dir)*.cpp))

#CC_SOURCES += $(foreach dir,$(SOURCES),$(wildcard $(dir)*.c))



# Project Build flags
WARNINGS:=-Wno-long-long -Wall -Wswitch-enum -pedantic
CXXFLAGS:=-pthread -std=gnu++98 $(WARNINGS)

CXXFLAGS	+= -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos -I../linux-deps/include -D RELEASE -D HOST_NO_REC -D TARGET_NACL32

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
CXX_32_OBJS:=$(RZDCY_FILES)
#$(patsubst %.cpp,%.nacl_86x32_build_obj,$(CXX_SOURCES))
#CC_32_OBJS:=$(patsubst %.c,%.build_obj_nacl_32,$(CC_SOURCES))

x86_32_OBJS:=$(CXX_32_OBJS) $(CC_32_OBJS)

%.o : %.cpp $(THIS_MAKE)
	$(CXX) -o $@ -c $< -m32 -O0 -g $(CXXFLAGS)
	
%.o : %.c $(THIS_MAKE)
	$(CC) -o $@ -c $< -m32 -O0 -g $(CXXFLAGS)

$(PROJECT)_x86_32.nexe : $(x86_32_OBJS)
	$(CXX) -o $@ $^ -m32 -O0 -g $(CXXFLAGS) $(LDFLAGS)

clean:
	rm $(x86_32_OBJS)

# Define a phony rule so it always runs, to build nexe and start up server.
.PHONY: RUN 
RUN: all
	python ../httpd.py


