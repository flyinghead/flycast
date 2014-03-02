
LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
FOR_ARM :=1
FOR_PANDORA :=1
USE_SDL :=1
USE_OSS :=1
#PGO_MAKE :=1
PGO_USE :=1

RZDCY_SRC_DIR = ../../newdc

include $(RZDCY_SRC_DIR)/newdc.mk

CC_PREFIX ?=

CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}strip

LD=${CC}

MFLAGS := -marm -march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp -funroll-loops -fpermissive
ASFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=softfp

LDFLAGS	:=	-g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common 

SOURCES	:=	cfg/ hw/arm7/ hw/aica/ hw/asic/ hw/ hw/gdrom/ hw/maple/ \
 hw/mem/ hw/pvr/ hw/sh4/ hw/sh4/rec_v2/ plugins/ profiler/ serial_ipc/ \
 hw/extdev/ hw/arm/ imgread/ linux/ linux-dist/ ./ rec-ARM/ deps/zlib/ deps/chdr/ deps/crypto/ arm_emitter/ 


CXXFLAGS :=  -std=gnu++11 -g -O3  -D RELEASE -c -D TARGET_PANDORA  -D WEIRD_SLOWNESS
CXXFLAGS += -frename-registers -fno-strict-aliasing -fsingle-precision-constant 
CXXFLAGS += -ffast-math -ftree-vectorize -fprefetch-loop-arrays  -Wno-write-strings
#-std=c++0x

CXXFLAGS += $(CFLAGS) $(MFLAGS) -fno-exceptions -fno-rtti
CXXFLAGS += -D SUPPORT_X11

ifdef USE_SDL
	CXXFLAGS += `sdl-config --cflags`
endif

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

INCS	:= -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos 
#-I../linux-deps/include

LIBS    := -L../linux-deps/lib
LIBS    += -lm -lrt -lEGL -lGLESv2 
#-lglslcompiler -lIMGegl -lpvr2d -lsrv_um 
LIBS    += -lpthread

ifdef USE_OSS
	CXXFLAGS += -DUSE_OSS
else
	LIBS += -lasound
endif

ifdef USE_SDL
	LIBS += `sdl-config --libs`
else
	LIBS    += -lX11 -lXdmcp -lXau
endif


OBJECTS=$(RZDCY_FILES:.cpp=.build_obj)
OBJECTS:=$(OBJECTS:.c=.build_obj)
OBJECTS:=$(OBJECTS:.S=.build_obj)
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj/%,$(OBJECTS))


EXECUTABLE_STRIPPED=nosym-reicast.elf
EXECUTABLE=reicast.elf

all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)
	echo $(RZDCY_FILES)

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
