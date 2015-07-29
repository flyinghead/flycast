
LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
#NO_REC := 1
#NO_REND := 1
WEBUI :=1 
USE_ALSA := 1
USE_OSS := 1
#USE_PULSEAUDIO := 1
USE_X11 := 1

CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}strip
LD=${CC}

MFLAGS := 
ASFLAGS := 
LDFLAGS :=
INCS :=
LIBS :=
CFLAGS := 
CXXFLAGS :=

# Platform auto-detection
# Can be overridden by using:
#   make platform=AMD64
ifeq (,$(platform))
    ARCH = $(shell uname -m)
    ifeq ($(ARCH), $(filter $(ARCH), i386 i686))
        platform = i686
    else ifeq ($(ARCH), $(filter $(ARCH), x86_64 AMD64))
        platform = AMD64
    else ifneq (,$(findstring arm,$(ARCH)))
        HARDWARE = $(shell grep Hardware /proc/cpuinfo)
        ifneq (,$(findstring BCM2709,$(HARDWARE)))
            platform = rpi2
        else ifneq (,$(findstring AM33XX,$(HARDWARE)))
            platform = beagle
        else ifneq (,$(findstring Pandora,$(HARDWARE)))
            platform = pandora
        else
            $(error Unsupported Hardware)
        endif
    else ifneq (,$(findstring mips,$(ARCH)))
        platform = gcwz
    else
        $(error Unsupported CPU architecture)
    endif
endif

$(info Platform: $(platform))

# Generic x86 (32 bit)
ifneq (,$(findstring i686,$(platform)))
    X86_REC := 1
    NOT_ARM := 1
    MFLAGS += -m32
    ASFLAGS += -32
    LDFLAGS += -m32
    CFLAGS += -m32 -D TARGET_LINUX_x86 -D TARGET_NO_AREC
    CXXFLAGS += -fno-exceptions

# Generic x86_64 (64 bit)
else ifneq (,$(findstring AMD64,$(platform)))
    X64_REC := 1
    NOT_ARM := 1
    CFLAGS += -D TARGET_LINUX_x64 -D TARGET_NO_AREC
    CXXFLAGS += -fexceptions

# Raspberry Pi 2
else ifneq (,$(findstring rpi2,$(platform)))
    AS=${CC_PREFIX}gcc
    MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -funroll-loops
    ASFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP
    INCS += -I/opt/vc/include/ -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I../linux-deps/include
    LIBS += -L/opt/vc/lib/  -L../linux-deps/lib -lbcm_host
    USE_GLES := 1

# BeagleBone Black
else ifneq (,$(findstring beagle,$(platform)))
    CC_PREFIX ?= arm-none-linux-gnueabi-
    MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -funroll-loops
    ASFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_BEAGLE
    USE_GLES := 1

# Pandora
else ifneq (,$(findstring pandora,$(platform)))
    FOR_PANDORA := 1
    USE_SDL := 1
    PGO_USE := 1
    USE_GLES := 1
    MFLAGS +== -marm -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp -funroll-loops -fpermissive
    ASFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
    CFLAGS += -D TARGET_PANDORA  -D WEIRD_SLOWNESS

# GCW Zero
else ifneq (,$(findstring gcwz,$(platform)))
    NOT_ARM := 1
    NO_REC := 1
    CC_PREFIX ?= /opt/gcw0-toolchain/usr/bin/mipsel-gcw0-linux-uclibc-
    CFLAGS += -D TARGET_GCW0 -D TARGET_NO_REC
    LIBS += -L../linux-deps/lib -L./enta_viv -lglapi
    GCWZ_PKG = reicast-gcwz.opk
    GCWZ_PKG_FILES = gcwz/default.gcw0.desktop gcwz/icon-32.png
else
    $(error Unknown platform)
endif

RZDCY_SRC_DIR = ../../core
include $(RZDCY_SRC_DIR)/core.mk

LDFLAGS += -g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common

CFLAGS += -g -O3 -D RELEASE -c -D USES_HOMEDIR #-D NO_REND
CFLAGS += -frename-registers -fno-strict-aliasing -fsingle-precision-constant 
CFLAGS += -ffast-math -ftree-vectorize

CXXFLAGS += -fno-rtti -fpermissive -fno-operator-names

INCS += -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos

LIBS += -lm -lrt -ldl
LIBS  += -lpthread

ifdef USE_SDL
    CXXFLAGS += `sdl-config --cflags`
    LIBS += `sdl-config --libs`
endif

ifdef PGO_MAKE
    CFLAGS += -fprofile-generate -pg
    LDFLAGS += -fprofile-generate
else
    CFLAGS += -fomit-frame-pointer
endif

ifdef PGO_USE
    CFLAGS += -fprofile-use
endif

ifdef LTO_TEST
    CFLAGS += -flto -fwhole-program 
    LDFLAGS +=-flto -fwhole-program 
endif

ifdef USE_X11
    CFLAGS += -D SUPPORT_X11
    CXXFLAGS += -D SUPPORT_X11 
    LIBS += -lX11
endif

ifdef USE_ALSA
    CXXFLAGS += -D USE_ALSA
    LIBS += -lasound
endif

ifdef USE_OSS
    CXXFLAGS += -D USE_OSS
endif

ifdef USE_PULSEAUDIO
    CXXFLAGS += -D USE_PULSEAUDIO
    LIBS += -lpulse-simple
endif

# GLES on x11?
ifdef USE_GLES
    CXXFLAGS += -DGLES
    LIBS += -lEGL -lGLESv2
else
    LIBS += -ldl -lGL #for desktop gl
endif


OBJECTS=$(RZDCY_FILES:.cpp=.build_obj)
OBJECTS:=$(OBJECTS:.c=.build_obj)
OBJECTS:=$(OBJECTS:.S=.build_obj)
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj-$(platform)/%,$(OBJECTS))

EXECUTABLE_STRIPPED=nosym-reicast.elf
EXECUTABLE=reicast.elf


all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)
ifneq (,$(findstring gcwz,$(platform)))
	mksquashfs $(EXECUTABLE_STRIPPED) $(GCWZ_PKG_FILES) $(GCWZ_PKG) -all-root
endif
	
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(MFLAGS) $(EXTRAFLAGS) $(LDFLAGS)  $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE_STRIPPED): $(EXECUTABLE)
	cp $< $@ && $(STRIP) $@

obj-$(platform)/%.build_obj : $(RZDCY_SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $(MFLAGS) $(CXXFLAGS) $< -o $@
	
obj-$(platform)/%.build_obj : $(RZDCY_SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $< -o $@

obj-$(platform)/%.build_obj : $(RZDCY_SRC_DIR)/%.S
	mkdir -p $(dir $@)	
	$(AS) $(ASFLAGS) $(INCS) $(CFLAGS) $< -o $@

clean:
	rm $(OBJECTS) $(EXECUTABLE) -f
