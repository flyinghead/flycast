RZDCY_FILES :=

RZDCY_SRC_DIR ?= $(call my-dir)
VERSION_HEADER := $(RZDCY_SRC_DIR)/version.h

RZDCY_MODULES	:=	cfg/ hw/arm7/ hw/aica/ hw/holly/ hw/ hw/gdrom/ hw/maple/ \
 hw/mem/ hw/pvr/ hw/sh4/ hw/sh4/interpr/ hw/sh4/modules/ profiler/ oslib/ \
 hw/naomi/ imgread/ ./ deps/libchdr/src/ deps/libchdr/deps/zlib-1.2.11/ \
 deps/libelf/ deps/chdpsr/ rend/ reios/ deps/xbrz/ \
 deps/imgui/ archive/ input/ log/ wsi/ network/ hw/bba/ debug/ \
 hw/modem/ deps/picotcp/modules/ deps/picotcp/stack/

ifndef NO_REC
	ifndef NOT_ARM
	    RZDCY_MODULES += rec-ARM/ deps/vixl/ deps/vixl/aarch32/
	endif
	
	ifdef X86_REC
	    RZDCY_MODULES += rec-x86/
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
    		RZDCY_FILES += $(RZDCY_SRC_DIR)/deps/glslang/glslang/OSDependent/Windows/ossource.cpp
    	else
    		RZDCY_FILES += $(RZDCY_SRC_DIR)/deps/glslang/glslang/OSDependent/Unix/ossource.cpp
    	endif
    endif
else
    RZDCY_MODULES += rend/norend/
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
	ifndef UNIT_TESTS
	    RZDCY_FILES += $(RZDCY_SRC_DIR)/windows/winmain.cpp
	endif
    RZDCY_FILES += $(RZDCY_SRC_DIR)/windows/win_vmem.cpp
    RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR)/deps/dirent
endif

ifdef USE_VULKAN
	ifdef FOR_WINDOWS
		RZDCY_CFLAGS += -DVK_USE_PLATFORM_WIN32_KHR
	else
		RZDCY_CFLAGS += -DVK_USE_PLATFORM_XLIB_KHR
	endif
	RZDCY_CFLAGS += -D USE_VULKAN
endif

RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR) -I$(RZDCY_SRC_DIR)/rend/gles -I$(RZDCY_SRC_DIR)/deps \
		 -I$(RZDCY_SRC_DIR)/deps/vixl -I$(RZDCY_SRC_DIR)/khronos -I$(RZDCY_SRC_DIR)/deps/glslang \
		 -I$(RZDCY_SRC_DIR)/deps/glm -I$(RZDCY_SRC_DIR)/deps/xbyak -I$(RZDCY_SRC_DIR)/deps/nowide/include \
		 -I$(RZDCY_SRC_DIR)/deps/picotcp/include -I$(RZDCY_SRC_DIR)/deps/picotcp/modules \
		 -I$(RZDCY_SRC_DIR)/deps/libchdr/include -I$(RZDCY_SRC_DIR)/deps/libchdr/deps/zlib-1.2.11/ \
		 -I$(RZDCY_SRC_DIR)/deps/libchdr/deps/lzma-19.00 -I$(RZDCY_SRC_DIR)/deps/libchdr/deps/lzma-19.00/include

ifdef USE_SYSTEM_MINIUPNPC
	RZDCY_CFLAGS += -I/usr/include/miniupnpc
else
	RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR)/deps/miniupnpc
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

RZDCY_CFLAGS += -DXXH_INLINE_ALL -I$(RZDCY_SRC_DIR)/deps/xxHash -I$(RZDCY_SRC_DIR)/deps/stb

RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cpp))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.cc))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.c))
RZDCY_FILES += $(foreach dir,$(addprefix $(RZDCY_SRC_DIR)/,$(RZDCY_MODULES)),$(wildcard $(dir)*.S))

ifdef STATIC_LIBZIP
RZDCY_CFLAGS += -I$(RZDCY_SRC_DIR)/deps/libzip/lib
LIBZIP_DIR = $(RZDCY_SRC_DIR)/deps/libzip/lib
RZDCY_FILES += $(LIBZIP_DIR)/zip_add.c \
	  $(LIBZIP_DIR)/zip_add_dir.c \
	  $(LIBZIP_DIR)/zip_add_entry.c \
	  $(LIBZIP_DIR)/zip_algorithm_deflate.c \
	  $(LIBZIP_DIR)/zip_buffer.c \
	  $(LIBZIP_DIR)/zip_close.c \
	  $(LIBZIP_DIR)/zip_delete.c \
	  $(LIBZIP_DIR)/zip_dir_add.c \
	  $(LIBZIP_DIR)/zip_dirent.c \
	  $(LIBZIP_DIR)/zip_discard.c \
	  $(LIBZIP_DIR)/zip_entry.c \
	  $(LIBZIP_DIR)/zip_error.c \
	  $(LIBZIP_DIR)/zip_error_clear.c \
	  $(LIBZIP_DIR)/zip_error_get.c \
	  $(LIBZIP_DIR)/zip_error_get_sys_type.c \
	  $(LIBZIP_DIR)/zip_error_strerror.c \
	  $(LIBZIP_DIR)/zip_error_to_str.c \
	  $(LIBZIP_DIR)/zip_extra_field.c \
	  $(LIBZIP_DIR)/zip_extra_field_api.c \
	  $(LIBZIP_DIR)/zip_fclose.c \
	  $(LIBZIP_DIR)/zip_fdopen.c \
	  $(LIBZIP_DIR)/zip_file_add.c \
	  $(LIBZIP_DIR)/zip_file_error_clear.c \
	  $(LIBZIP_DIR)/zip_file_error_get.c \
	  $(LIBZIP_DIR)/zip_file_get_comment.c \
	  $(LIBZIP_DIR)/zip_file_get_external_attributes.c \
	  $(LIBZIP_DIR)/zip_file_get_offset.c \
	  $(LIBZIP_DIR)/zip_file_rename.c \
	  $(LIBZIP_DIR)/zip_file_replace.c \
	  $(LIBZIP_DIR)/zip_file_set_comment.c \
	  $(LIBZIP_DIR)/zip_file_set_encryption.c \
	  $(LIBZIP_DIR)/zip_file_set_external_attributes.c \
	  $(LIBZIP_DIR)/zip_file_set_mtime.c \
	  $(LIBZIP_DIR)/zip_file_strerror.c \
	  $(LIBZIP_DIR)/zip_fopen.c \
	  $(LIBZIP_DIR)/zip_fopen_encrypted.c \
	  $(LIBZIP_DIR)/zip_fopen_index.c \
	  $(LIBZIP_DIR)/zip_fopen_index_encrypted.c \
	  $(LIBZIP_DIR)/zip_fread.c \
	  $(LIBZIP_DIR)/zip_fseek.c \
	  $(LIBZIP_DIR)/zip_ftell.c \
	  $(LIBZIP_DIR)/zip_get_archive_comment.c \
	  $(LIBZIP_DIR)/zip_get_archive_flag.c \
	  $(LIBZIP_DIR)/zip_get_encryption_implementation.c \
	  $(LIBZIP_DIR)/zip_get_file_comment.c \
	  $(LIBZIP_DIR)/zip_get_name.c \
	  $(LIBZIP_DIR)/zip_get_num_entries.c \
	  $(LIBZIP_DIR)/zip_get_num_files.c \
	  $(LIBZIP_DIR)/zip_hash.c \
	  $(LIBZIP_DIR)/zip_io_util.c \
	  $(LIBZIP_DIR)/zip_libzip_version.c \
	  $(LIBZIP_DIR)/zip_memdup.c \
	  $(LIBZIP_DIR)/zip_name_locate.c \
	  $(LIBZIP_DIR)/zip_new.c \
	  $(LIBZIP_DIR)/zip_open.c \
	  $(LIBZIP_DIR)/zip_pkware.c \
	  $(LIBZIP_DIR)/zip_progress.c \
	  $(LIBZIP_DIR)/zip_rename.c \
	  $(LIBZIP_DIR)/zip_replace.c \
	  $(LIBZIP_DIR)/zip_set_archive_comment.c \
	  $(LIBZIP_DIR)/zip_set_archive_flag.c \
	  $(LIBZIP_DIR)/zip_set_default_password.c \
	  $(LIBZIP_DIR)/zip_set_file_comment.c \
	  $(LIBZIP_DIR)/zip_set_file_compression.c \
	  $(LIBZIP_DIR)/zip_set_name.c \
	  $(LIBZIP_DIR)/zip_source_accept_empty.c \
	  $(LIBZIP_DIR)/zip_source_begin_write.c \
	  $(LIBZIP_DIR)/zip_source_begin_write_cloning.c \
	  $(LIBZIP_DIR)/zip_source_buffer.c \
	  $(LIBZIP_DIR)/zip_source_call.c \
	  $(LIBZIP_DIR)/zip_source_close.c \
	  $(LIBZIP_DIR)/zip_source_commit_write.c \
	  $(LIBZIP_DIR)/zip_source_compress.c \
	  $(LIBZIP_DIR)/zip_source_crc.c \
	  $(LIBZIP_DIR)/zip_source_error.c \
	  $(LIBZIP_DIR)/zip_source_file_common.c \
	  $(LIBZIP_DIR)/zip_source_file_stdio.c \
	  $(LIBZIP_DIR)/zip_source_free.c \
	  $(LIBZIP_DIR)/zip_source_function.c \
	  $(LIBZIP_DIR)/zip_source_get_file_attributes.c \
	  $(LIBZIP_DIR)/zip_source_is_deleted.c \
	  $(LIBZIP_DIR)/zip_source_layered.c \
	  $(LIBZIP_DIR)/zip_source_open.c \
	  $(LIBZIP_DIR)/zip_source_pkware_decode.c \
	  $(LIBZIP_DIR)/zip_source_pkware_encode.c \
	  $(LIBZIP_DIR)/zip_source_read.c \
	  $(LIBZIP_DIR)/zip_source_remove.c \
	  $(LIBZIP_DIR)/zip_source_rollback_write.c \
	  $(LIBZIP_DIR)/zip_source_seek.c \
	  $(LIBZIP_DIR)/zip_source_seek_write.c \
	  $(LIBZIP_DIR)/zip_source_stat.c \
	  $(LIBZIP_DIR)/zip_source_supports.c \
	  $(LIBZIP_DIR)/zip_source_tell.c \
	  $(LIBZIP_DIR)/zip_source_tell_write.c \
	  $(LIBZIP_DIR)/zip_source_window.c \
	  $(LIBZIP_DIR)/zip_source_write.c \
	  $(LIBZIP_DIR)/zip_source_zip.c \
	  $(LIBZIP_DIR)/zip_source_zip_new.c \
	  $(LIBZIP_DIR)/zip_stat.c \
	  $(LIBZIP_DIR)/zip_stat_index.c \
	  $(LIBZIP_DIR)/zip_stat_init.c \
	  $(LIBZIP_DIR)/zip_strerror.c \
	  $(LIBZIP_DIR)/zip_string.c \
	  $(LIBZIP_DIR)/zip_unchange.c \
	  $(LIBZIP_DIR)/zip_unchange_all.c \
	  $(LIBZIP_DIR)/zip_unchange_archive.c \
	  $(LIBZIP_DIR)/zip_unchange_data.c \
	  $(LIBZIP_DIR)/zip_utf-8.c \
	  $(LIBZIP_DIR)/zip_err_str.c

	  ifdef FOR_WINDOWS
		  RZDCY_FILES += $(LIBZIP_DIR)/zip_source_file_win32.c \
		  		$(LIBZIP_DIR)/zip_source_file_win32_named.c \
			    $(LIBZIP_DIR)/zip_source_file_win32_utf16.c \
			    $(LIBZIP_DIR)/zip_source_file_win32_utf8.c \
				$(LIBZIP_DIR)/zip_source_file_win32_ansi.c \
				$(LIBZIP_DIR)/zip_random_win32.c
	  else
		  RZDCY_FILES += $(LIBZIP_DIR)/zip_mkstempm.c \
				$(LIBZIP_DIR)/zip_source_file_stdio_named.c \
				$(LIBZIP_DIR)/zip_random_unix.c
	  endif
endif

$(VERSION_HEADER):
	echo "#define REICAST_VERSION \"`git describe --tags --always`\"" > $(VERSION_HEADER)
	echo "#define GIT_HASH \"`git rev-parse --short HEAD`\"" >> $(VERSION_HEADER)
	echo "#define BUILD_DATE \"`date '+%Y-%m-%d %H:%M:%S %Z'`\"" >> $(VERSION_HEADER)

