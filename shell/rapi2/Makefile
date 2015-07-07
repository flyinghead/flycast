LOCAL_PATH := $(call my-dir)
FOR_LINUX :=1
WEBUI :=1
#NO_REND := 1
#NO_X11 := 1
RZDCY_SRC_DIR = ../../core

include $(RZDCY_SRC_DIR)/core.mk

#CC_PREFIX ?= arm-none-linux-gnueabi-

CXX=${CC_PREFIX}g++
CC=${CC_PREFIX}gcc
AS=${CC_PREFIX}gcc
STRIP=${CC_PREFIX}strip

LD=${CC}

MFLAGS := -marm -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -funroll-loops
ASFLAGS := -march=armv7-a -mfpu=neon -mfloat-abi=softfp

LDFLAGS	:=	-g -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common 


CXXFLAGS := -g -O3  -D RELEASE -c -D TARGET_BEAGLE -D TARGET_LINUX_ARMELv7 -DUSE_ALSA  
CXXFLAGS += -frename-registers -fno-strict-aliasing -fsingle-precision-constant 
CXXFLAGS += -ffast-math -ftree-vectorize -fprefetch-loop-arrays 
#-std=c++0x

CXXFLAGS += $(CFLAGS) $(MFLAGS) -fno-exceptions -fno-rtti
CXXFLAGS +=  -DARM_HARDFP -DGLES


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

INCS	:= -I/opt/vc/include/ -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads  -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/deps -I$(RZDCY_SRC_DIR)/khronos -I../linux-deps/include

LIBS    :=  -L/opt/vc/lib/  -L../linux-deps/lib
LIBS    += -lm -lrt -ldl -lEGL -lGLESv2 #-lglslcompiler -lIMGegl -lpvr2d -lsrv_um 
LIBS    += -lpthread -lasound -lX11 -lXdmcp -lXau  -lbcm_host


OBJECTS=$(RZDCY_FILES:.cpp=.build_obj)
OBJECTS:=$(OBJECTS:.c=.build_obj)
OBJECTS:=$(OBJECTS:.S=.build_obj)
OBJECTS:=$(patsubst $(RZDCY_SRC_DIR)/%,obj/%,$(OBJECTS))


EXECUTABLE_STRIPPED=nosym-reicast.elf
EXECUTABLE=reicast.elf

all: $(CPPFILES) $(EXECUTABLE) $(EXECUTABLE_STRIPPED)

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
	$(AS) $(ASFLAGS) $(INCS) $(CXXFLAGS) $< -o $@


clean:
	rm $(OBJECTS) $(EXECUTABLE) -f
