
LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
NOT_ARM := 1
NO_REC := 1
#NO_REND := 1

RZDCY_SRC_DIR = ../../core

include $(RZDCY_SRC_DIR)/core.mk

CC_PREFIX ?= /opt/gcw0-toolchain/usr/bin/mipsel-gcw0-linux-uclibc-

CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}strip

LD=${CC}

MFLAGS := 
#-marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -funroll-loops
ASFLAGS := 
#-march=armv7-a -mfpu=neon -mfloat-abi=softfp

LDFLAGS	:=	-g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common 

SOURCES	:=	cfg/ hw/arm7/ hw/aica/ hw/asic/ hw/ hw/gdrom/ hw/maple/ \
 hw/mem/ hw/pvr/ hw/sh4/ hw/sh4/rec_v2/ plugins/ profiler/ serial_ipc/ \
 hw/extdev/ hw/arm/ imgread/ linux/ linux-dist/ ./ rec-ARM/ deps/zlib/ deps/chdr/ deps/crypto/ arm_emitter/ 


CXXFLAGS := -g -O3  -D RELEASE -c -D TARGET_GCW0 -D USES_HOMEDIR -D HOST_NO_REC #-D NO_REND
CXXFLAGS += -frename-registers -fno-strict-aliasing -fsingle-precision-constant 
CXXFLAGS += -ffast-math -ftree-vectorize 
#-fprefetch-loop-arrays 
#-std=c++0x

CXXFLAGS += $(CFLAGS) $(MFLAGS) -fno-exceptions -fno-rtti
#CXXFLAGS += -D SUPPORT_X11


ifdef PGO_MAKE
	CXXFLAGS += -fprofile-generate -pg
	LDFLAGS	+= -fprofile-generate
else
	CXXFLAGS += -fomit-frame-pointer
endif

ifdef PGO_USE
	CXXFLAGS += -fprofile-use
endif


ifdef LTO_TEST
	CXXFLAGS += -flto -fwhole-program 
	LDFLAGS +=-flto -fwhole-program 
endif

INCS	:= -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos -I../linux-deps/include

LIBS    := -L../linux-deps/lib -L./enta_viv 
LIBS	+= -lglapi
LIBS    += -lm -lrt -lEGL -lGLESv2 #-lglslcompiler -lIMGegl -lpvr2d -lsrv_um
LIBS    += -lpthread -lasound #-lX11 -lXdmcp -lXau


OBJECTS=$(RZDCY_FILES:.cpp=.build_obj)
OBJECTS:=$(OBJECTS:.c=.build_obj)
OBJECTS:=$(OBJECTS:.S=.build_obj)
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj/%,$(OBJECTS))


PACKAGE=reicast-gcwz.opk
EXECUTABLE_STRIPPED=nosym-reicast.elf
EXECUTABLE_TARBZ=reicast.tar.bz2
EXECUTABLE=reicast.elf

PACKAGE_FILES=$(EXECUTABLE_STRIPPED) default.gcw0.desktop icon-32.png

all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)  $(PACKAGE)
	
$(PACKAGE): $(PACKAGE_FILES)
	mksquashfs $(PACKAGE_FILES) $(PACKAGE) -all-root

$(EXECUTABLE): $(OBJECTS)
	echo $(RZDCY_FILES)
	$(CXX) $(MFLAGS) $(EXTRAFLAGS) $(LDFLAGS)  $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE_STRIPPED): $(EXECUTABLE)
	cp $< $@ && $(STRIP) $@

obj/%.build_obj : $(RZDCY_SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(EXTRAFLAGS) $(INCS) $(CXXFLAGS) $< -o $@
	
obj/%.build_obj : $(RZDCY_SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(EXTRAFLAGS) $(INCS) $(CXXFLAGS) $< -o $@

obj/%.build_obj : $(RZDCY_SRC_DIR)/%.S
	mkdir -p $(dir $@)	
	$(AS) $(ASFLAGS) $(INCS) $< -o $@


clean:
	rm $(OBJECTS) $(EXECUTABLE) -f
