
LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
NOT_ARM := 1
X86_REC := 1
#NO_REC := 1
#NO_REND := 1
WEBUI :=1 

RZDCY_SRC_DIR = ../../core

include $(RZDCY_SRC_DIR)/core.mk


CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}as
STRIP=${CC_PREFIX}strip

LD=${CC}

MFLAGS := -m32
#-marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=softfp -funroll-loops
ASFLAGS := -32
#-march=armv7-a -mfpu=neon -mfloat-abi=softfp

LDFLAGS	:= -m32	-g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common 

SOURCES	:=	cfg/ hw/arm7/ hw/aica/ hw/asic/ hw/ hw/gdrom/ hw/maple/ \
 hw/mem/ hw/pvr/ hw/sh4/ hw/sh4/rec_v2/ plugins/ profiler/ serial_ipc/ \
 hw/extdev/ hw/arm/ imgread/ linux/ linux-dist/ ./ rec-x86/ deps/zlib/ deps/chdr/ deps/crypto/ arm_emitter/


CFLAGS :=  -m32 -g -O3  -D RELEASE -c -D TARGET_LINUX_x86 -D USES_HOMEDIR -D HOST_NO_AREC #-D NO_REND
CFLAGS += -D SUPPORT_X11
CFLAGS += -frename-registers -fno-strict-aliasing -fsingle-precision-constant 
CFLAGS += -ffast-math -ftree-vectorize 


#-fprefetch-loop-arrays 
#-std=c++0x
CXXFLAGS += $(CFLAGS) $(MFLAGS) -fno-exceptions -fno-rtti -fpermissive
CXXFLAGS += -D SUPPORT_X11 

# use this to do GLES on x11 (also, update the libs)
# CXXFLAGS += -DGLES


ifdef PGO_MAKE
	CFLAGS += -fprofile-generate -pg
	LDFLAGS	+= -fprofile-generate
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

INCS	:= -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos -I../linux-deps/include

LIBS    := -L../linux-deps/lib/x86 -L./enta_viv 
#LIBS	+= -lglapi
LIBS    += -lm -lrt
LIBS	+= -ldl -lGL #for desktop gl
#use this for GLES 
#LIBS	+= -lEGL -lGLESv2 #-lglslcompiler -lIMGegl -lpvr2d -lsrv_um
LIBS    += -lpthread -lasound -lX11


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
