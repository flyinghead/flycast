# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH:= $(call my-dir)/..

include $(CLEAR_VARS)

FOR_ANDROID := 1
USE_GLES := 1
CHD5_LZMA := 1
CHD5_FLAC := 1
USE_MODEM := 1
USE_VULKAN = 1

ifneq ($(TARGET_ARCH_ABI),armeabi-v7a)
  NOT_ARM := 1
else
  NOT_ARM := 
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
#  CPP_REC := 1
  ARM64_REC := 1
  ISARM64 := 1
else
#  CPP_REC :=
  ARM64_REC :=
  ISARM64 :=
endif

ifeq ($(TARGET_ARCH_ABI),x86)
  X86_REC := 1
else
  X86_REC := 
endif

ifeq ($(TARGET_ARCH_ABI),mips)
  ISMIPS := 1
  NO_REC := 1
else
  ISMIPS :=
  NO_REC :=
endif

$(info $$TARGET_ARCH_ABI is [${TARGET_ARCH_ABI}])

include $(LOCAL_PATH)/../../../../../core/core.mk

LOCAL_SRC_FILES := $(RZDCY_FILES)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/jni/src/Android.cpp)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/jni/src/utils.cpp)
LOCAL_CFLAGS  := $(RZDCY_CFLAGS) -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -DVK_USE_PLATFORM_ANDROID_KHR #-DDEBUGFAST
LOCAL_CPPFLAGS  := $(RZDCY_CXXFLAGS) -fPIC -fvisibility=hidden -fvisibility-inlines-hidden -ffunction-sections -fdata-sections -fexceptions

# 7-Zip/LZMA settings (CHDv5)
ifdef CHD5_LZMA
	LOCAL_CFLAGS += -D_7ZIP_ST -DCHD5_LZMA
endif

# FLAC settings (CHDv5)
ifdef CHD5_FLAC
	LOCAL_CFLAGS += -DCHD5_FLAC
endif

LOCAL_CFLAGS += -DGLES3
LOCAL_CPPFLAGS += -std=c++11 -fopenmp
LOCAL_LDFLAGS  += -fopenmp

ifeq ($(TARGET_ARCH_ABI),x86)
  LOCAL_CFLAGS+= -DTARGET_NO_AREC -DTARGET_NO_OPENMP
endif

LOCAL_CPP_FEATURES := 
# LOCAL_SHARED_LIBRARIES:= libcutils libutils
LOCAL_PRELINK_MODULE  := false

LOCAL_MODULE	:= dc
LOCAL_DISABLE_FORMAT_STRING_CHECKS=true
LOCAL_ASFLAGS := -fPIC -fvisibility=hidden
LOCAL_LDLIBS	:= -llog -lEGL -lz -landroid
#-Wl,-Map,./res/raw/syms.mp3
LOCAL_ARM_MODE	:= arm

ifeq ($(TARGET_ARCH_ABI),mips)
  LOCAL_LDFLAGS += -Wl,--gc-sections
else
  ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_LDFLAGS += -Wl,--gc-sections
  else
    LOCAL_LDFLAGS += -Wl,--gc-sections,--icf=safe
    LOCAL_LDLIBS +=  -Wl,--no-warn-shared-textrel
  endif
endif

$(LOCAL_SRC_FILES): $(VERSION_HEADER)

include $(BUILD_SHARED_LIBRARY)
