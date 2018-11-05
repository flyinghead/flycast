#LOCAL_PATH:= 

#MFLAGS	:= -marm -march=armv7-a -mtune=cortex-a8 -mfpu=vfpv3-d16 -mfloat-abi=softfp
#ASFLAGS	:= -march=armv7-a -mfpu=vfp-d16 -mfloat-abi=softfp
#LDFLAGS	:= -Wl,-Map,$(notdir $@).map,--gc-sections -Wl,-O3 -Wl,--sort-common 

RZDCY_SRC_DIR ?= $(call my-dir)

RZDCY_MODULES	:=	cfg/ hw/arm7/ hw/aica/ hw/holly/ hw/ hw/gdrom/ hw/maple/ hw/modem/ \
 hw/mem/ hw/pvr/ hw/sh4/ hw/sh4/interpr/ hw/sh4/modules/ plugins/ profiler/ oslib/ \
 hw/extdev/ hw/arm/ hw/naomi/ imgread/ ./ deps/coreio/ deps/zlib/ deps/chdr/ deps/crypto/ \
 deps/libelf/ deps/chdpsr/ arm_emitter/ rend/ reios/ deps/libpng/ deps/xbrz/ \
 deps/picotcp/modules/ deps/picotcp/stack/ deps/xxhash/ deps/libzip/

ifdef CHD5_LZMA
	RZDCY_MODULES += deps/lzma/
endif

ifdef CHD5_FLAC
	RZDCY_MODULES += deps/flac/src/libFLAC/
endif

ifdef WEBUI
	RZDCY_MODULES += webui/
	RZDCY_MODULES += deps/libwebsocket/

	ifdef FOR_ANDROID
		RZDCY_MODULES += deps/ifaddrs/
	endif
endif

ifndef NO_REC
	RZDCY_MODULES += hw/sh4/dyna/
endif

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

ifndef NO_REND
    RZDCY_MODULES += rend/gles/
    ifndef USE_GLES
	ifndef USE_DISPMANX
	    RZDCY_MODULES += rend/gl4/
	endif
    endif
else
    RZDCY_MODULES += rend/norend/
endif

ifdef HAS_SOFTREND
	RZDCY_MODULES += rend/soft/
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
    RZDCY_MODULES += linux-dist/ linux/
endif

ifdef FOR_WINDOWS
    RZDCY_MODULES += windows/
endif

RZDCY_FILES := $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cpp))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.c))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.S))

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
			ifndef ISMIPS
				RZDCY_CFLAGS += -DTARGET_LINUX_x86
			else
				RZDCY_CFLAGS += -DTARGET_LINUX_MIPS
			endif
		endif
	else
RZDCY_CFLAGS := 
	endif
endif

RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/rend/gles -I$(RZDCY_SRC_DIR)/deps \
		-I$(RZDCY_SRC_DIR)/deps/picotcp/include -I$(RZDCY_SRC_DIR)/deps/picotcp/modules

ifdef NO_REC
  RZDCY_CFLAGS += -DTARGET_NO_REC
endif

ifdef USE_GLES
  RZDCY_CFLAGS += -DGLES -fPIC
endif

ifdef HAS_SOFTREND
	RZDCY_CFLAGS += -DTARGET_SOFTREND
endif

ifdef CHD5_FLAC
	RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR)/deps/flac/src/libFLAC/include/ -I$(RZDCY_SRC_DIR)/deps/flac/include
	RZDCY_CFLAGS += -DPACKAGE_VERSION=\"1.3.2\" -DFLAC__HAS_OGG=0 -DFLAC__NO_DLL -DHAVE_LROUND -DHAVE_STDINT_H -DHAVE_STDLIB_H -DHAVE_SYS_PARAM_H
endif

RZDCY_CXXFLAGS := $(RZDCY_CFLAGS) -fno-exceptions -fno-rtti -std=gnu++11
