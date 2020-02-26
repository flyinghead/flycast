#LOCAL_PATH:=

#MFLAGS	:= -marm -march=armv7-a -mtune=cortex-a8 -mfpu=vfpv3-d16 -mfloat-abi=softfp
#ASFLAGS	:= -march=armv7-a -mfpu=vfp-d16 -mfloat-abi=softfp
#LDFLAGS	:= -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common

RZDCY_SRC_DIR ?= $(call my-dir)
VERSION_HEADER := $(RZDCY_SRC_DIR)/version.h

RZDCY_MODULES	:=	cfg/ hw/arm7/ hw/aica/ hw/holly/ hw/ hw/gdrom/ hw/maple/ \
 hw/mem/ hw/pvr/ hw/sh4/ hw/sh4/interpr/ hw/sh4/modules/ plugins/ profiler/ oslib/ \
 hw/extdev/ hw/arm/ hw/naomi/ imgread/ ./ deps/coreio/ deps/zlib/ deps/chdr/ deps/crypto/ \
 deps/libelf/ deps/chdpsr/ arm_emitter/ rend/ reios/ deps/libpng/ deps/xbrz/ \
 deps/libzip/ deps/imgui/ archive/ input/ log/ wsi/

ifndef NOT_ARM
    RZDCY_MODULES += rec-ARM/
endif

ifdef X86_REC
    RZDCY_MODULES += rec-x86/ emitter/
endif

ifdef X64_REC
    RZDCY_MODULES += rec-x64/
endif

ifdef CPP_REC
    RZDCY_MODULES += rec-cpp/
endif

ifdef ARM64_REC
    RZDCY_MODULES += rec-ARM64/ deps/vixl/ deps/vixl/aarch64/
endif

ifndef NO_REND
    RZDCY_MODULES += rend/gles/
    ifndef USE_GLES
	ifndef USE_DISPMANX
	    RZDCY_MODULES += rend/gl4/
	endif
    endif
    ifdef USE_VULKAN
    	RZDCY_MODULES += rend/vulkan/ rend/vulkan/oit/ deps/volk/ \
    		deps/glslang/glslang/MachineIndependent/ \
    		deps/glslang/glslang/MachineIndependent/preprocessor/ \
    		deps/glslang/glslang/GenericCodeGen/ \
    		deps/glslang/OGLCompilersDLL/ \
    		deps/glslang/SPIRV/
    	ifdef FOR_WINDOWS
    		RZDCY_MODULES += deps/glslang/glslang/OSDependent/Windows/
    	else
    		RZDCY_MODULES += deps/glslang/glslang/OSDependent/Unix/
    	endif
    endif
else
    RZDCY_MODULES += rend/norend/
endif

ifndef NO_NIXPROF
    RZDCY_MODULES += linux/nixprof/
endif

ifdef FOR_ANDROID
    RZDCY_MODULES += android/ deps/libandroid/ linux/
endif

ifdef USE_SDL
    RZDCY_MODULES += sdl/
endif

ifdef FOR_LINUX
	ifndef UNIT_TESTS
    	RZDCY_MODULES += linux-dist/
	endif
    RZDCY_MODULES += linux/
endif

ifdef FOR_WINDOWS
    RZDCY_MODULES += windows/
endif

ifdef FOR_PANDORA
RZDCY_CFLAGS	:= \
	$(CFLAGS) -c -O3 \
	-DRELEASE -DPANDORA\
	-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp \
	-frename-registers -fsingle-precision-constant -ffast-math \
	-ftree-vectorize -fomit-frame-pointer
	RZDCY_CFLAGS += -march=armv7-a -mtune=cortex-a8 -mfpu=neon
	RZDCY_CFLAGS += -DTARGET_LINUX_ARMELv7
else
	ifdef FOR_ANDROID
RZDCY_CFLAGS	:= \
		$(CFLAGS) -c -O3 \
		-D_ANDROID -DRELEASE \
		-frename-registers -fsingle-precision-constant -ffast-math \
		-ftree-vectorize -fomit-frame-pointer

		ifndef NOT_ARM
			RZDCY_CFLAGS += -march=armv7-a -mtune=cortex-a9 -mfpu=vfpv3-d16
			RZDCY_CFLAGS += -DTARGET_LINUX_ARMELv7
		else
			ifdef ISARM64
				RZDCY_CFLAGS += -march=armv8-a
				RZDCY_CFLAGS += -DTARGET_LINUX_ARMv8
			else
				ifdef ISMIPS
					RZDCY_CFLAGS += -DTARGET_LINUX_MIPS
				else
					RZDCY_CFLAGS += -DTARGET_LINUX_x86
				endif
			endif
		endif
	else
RZDCY_CFLAGS := 
	endif
endif

ifdef USE_VULKAN
	ifdef FOR_WINDOWS
		RZDCY_CFLAGS += -DVK_USE_PLATFORM_WIN32_KHR
	else
		ifdef FOR_ANDROID
			RZDCY_CFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
		else
			RZDCY_CFLAGS += -DVK_USE_PLATFORM_XLIB_KHR
		endif
	endif
	RZDCY_CFLAGS += -D USE_VULKAN
endif

RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/rend/gles -I$(RZDCY_SRC_DIR)/deps \
		 -I$(RZDCY_SRC_DIR)/deps/vixl -I$(RZDCY_SRC_DIR)/khronos -I$(RZDCY_SRC_DIR)/deps/glslang

ifdef USE_MODEM
	RZDCY_CFLAGS += -DENABLE_MODEM -I$(RZDCY_SRC_DIR)/deps/picotcp/include -I$(RZDCY_SRC_DIR)/deps/picotcp/modules
	RZDCY_MODULES += hw/modem/ deps/picotcp/modules/ deps/picotcp/stack/
endif

ifdef NO_REC
	RZDCY_CFLAGS += -DTARGET_NO_REC
else
	RZDCY_MODULES += hw/sh4/dyna/
endif

ifdef USE_GLES
  RZDCY_CFLAGS += -DGLES -fPIC
endif

ifdef CHD5_FLAC
	RZDCY_CFLAGS += -DCHD5_FLAC -I$(RZDCY_SRC_DIR)/deps/flac/src/libFLAC/include/ -I$(RZDCY_SRC_DIR)/deps/flac/include
	RZDCY_CFLAGS += -DHAVE_CONFIG_H
	RZDCY_MODULES += deps/flac/src/libFLAC/
endif

# 7-Zip/LZMA settings (CHDv5)
ifdef CHD5_LZMA
	RZDCY_MODULES += deps/lzma/
	RZDCY_CFLAGS += -D_7ZIP_ST -DCHD5_LZMA
endif

RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR)/deps/libpng -I$(RZDCY_SRC_DIR)/deps/libzip -I$(RZDCY_SRC_DIR)/deps/zlib
RZDCY_CFLAGS +=  -DXXH_INLINE_ALL -I$(RZDCY_SRC_DIR)/deps/xxhash

RZDCY_CXXFLAGS := $(RZDCY_CFLAGS) -fno-exceptions -fno-rtti -std=gnu++11

RZDCY_FILES := $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cpp))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cc))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.c))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.S))

$(VERSION_HEADER):
	echo "#define REICAST_VERSION \"`git describe --tags --always`\"" > $(VERSION_HEADER)
	echo "#define GIT_HASH \"`git rev-parse --short HEAD`\"" >> $(VERSION_HEADER)
	echo "#define BUILD_DATE \"`date '+%Y-%m-%d %H:%M:%S %Z'`\"" >> $(VERSION_HEADER)

