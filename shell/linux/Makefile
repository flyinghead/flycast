
LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
#NO_REC := 1
#NO_REND := 1
WEBUI :=1 
USE_ALSA := 1
USE_OSS := 1
#USE_PULSEAUDIO := 1
USE_X11 := 1

MFLAGS := 
ASFLAGS := 
LDFLAGS :=
INCS :=
LIBS :=
CFLAGS := 
CXXFLAGS :=

ifeq (,$(ARCH))
    ARCH = $(shell uname -m)
endif

$(info Building arch: $(ARCH))
# x86 (32 Bit)
ifeq ($(ARCH), $(filter $(ARCH), i386 i686))
    X86_REC := 1
    NOT_ARM := 1
    MFLAGS += -m32
    ASFLAGS += -32
    LDFLAGS += -m32
    CFLAGS += -m32 -D TARGET_LINUX_x86 -D TARGET_NO_AREC
    CXXFLAGS += -fno-exceptions

# x86_64 (64 Bit)
else ifeq ($(ARCH), $(filter $(ARCH), x86_64 AMD64))
    X64_REC := 1
    NOT_ARM := 1
    CFLAGS += -D TARGET_LINUX_x64 -D TARGET_NO_AREC
    CXXFLAGS += -fexceptions

# ARM
else ifneq (,$(findstring arm,$(ARCH)))
    ifeq (,$(HARDWARE))
        HARDWARE = $(shell grep Hardware /proc/cpuinfo)
    endif

    # Raspberry Pi 2
    ifneq (,$(findstring BCM2709,$(HARDWARE)))
        MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -funroll-loops
        ASFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
        CFLAGS += -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DARM_HARDFP
        INCS += -I/opt/vc/include/ -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads -I../linux-deps/include
				LIBS += -L/opt/vc/lib/  -L../linux-deps/lib -lbcm_host
        USE_GLES := 1

    # BeagleBone Black
    else ifneq (,$(findstring AM33XX,$(HARDWARE)))
        CC_PREFIX ?= arm-none-linux-gnueabi-
        MFLAGS += -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -funroll-loops
        ASFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=softfp
        CFLAGS += -D TARGET_BEAGLE
        USE_GLES := 1

    # Pandora
    else ifneq (,$(findstring Pandora,$(HARDWARE)))
    	  FOR_ARM := 1
        FOR_PANDORA := 1
        USE_SDL := 1
        PGO_USE := 1
        USE_GLES := 1
        MFLAGS +== -marm -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp -funroll-loops -fpermissive
        ASFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=softfp
        CFLAGS += -D TARGET_PANDORA  -D WEIRD_SLOWNESS

    else
        $(error Unsupported Hardware)
    endif
else
    $(error Unsupported CPU architecture)
endif

RZDCY_SRC_DIR = ../../core
include $(RZDCY_SRC_DIR)/core.mk

CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}strip
LD=${CC}

LDFLAGS += -g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common

CFLAGS += -g -O3 -D RELEASE -c -D USES_HOMEDIR #-D NO_REND
CFLAGS += -frename-registers -fno-strict-aliasing -fsingle-precision-constant 
CFLAGS += -ffast-math -ftree-vectorize

CXXFLAGS += $(CFLAGS) $(MFLAGS) -fno-rtti -fpermissive -fno-operator-names

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
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj/%,$(OBJECTS))


EXECUTABLE_STRIPPED=nosym-reicast.elf
EXECUTABLE=reicast.elf

PACKAGE_FILES=$(EXECUTABLE_STRIPPED) default.gcw0.desktop icon-32.png

all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)
	
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(MFLAGS) $(EXTRAFLAGS) $(LDFLAGS)  $(OBJECTS) $(LIBS) -o $@

$(EXECUTABLE_STRIPPED): $(EXECUTABLE)
	cp $< $@ && $(STRIP) $@

obj/%.build_obj : $(RZDCY_SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $(CXXFLAGS) $< -o $@
	
obj/%.build_obj : $(RZDCY_SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(EXTRAFLAGS) $(INCS) $(CFLAGS) $< -o $@

obj/%.build_obj : $(RZDCY_SRC_DIR)/%.S
	mkdir -p $(dir $@)	
	$(AS) $(ASFLAGS) $(INCS) $< -o $@


clean:
	rm $(OBJECTS) $(EXECUTABLE) -f
