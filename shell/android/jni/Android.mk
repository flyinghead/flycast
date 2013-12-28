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

include $(LOCAL_PATH)/../../core/core.mk



LOCAL_SRC_FILES := $(RZDCY_FILES) $(wildcard $(LOCAL_PATH)/jni/src/*.cpp)
LOCAL_CXXFLAGS  := $(RZDCY_CXXFLAGS)

LOCAL_SHARED_LIBRARIES:= libcutils libutils
LOCAL_PRELINK_MODULE  := false

LOCAL_MODULE	:= dc
LOCAL_CFLAGS	:= $(LOCAL_CXXFLAGS) -DHAS_VMU
LOCAL_DISABLE_FORMAT_STRING_CHECKS=true
LOCAL_ASFLAGS := -fvisibility=hidden
LOCAL_LDLIBS	:= -llog -lGLESv2 -lEGL -lz 
#-Wl,-Map,./res/raw/syms.mp3
LOCAL_ARM_MODE	:= arm

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
