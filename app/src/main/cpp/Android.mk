LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_ARCH_ABI),x86)
    include $(CLEAR_VARS)
    LOCAL_MODULE := dobby
    LOCAL_SRC_FILES := $(LOCAL_PATH)/dobby/x86/libdobby.a
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/dobby
    include $(PREBUILT_STATIC_LIBRARY)
endif

ifeq ($(TARGET_ARCH_ABI),x86_64)
    include $(CLEAR_VARS)
    LOCAL_MODULE := dobby
    LOCAL_SRC_FILES := $(LOCAL_PATH)/dobby/x86_64/libdobby.a
    LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/dobby
    include $(PREBUILT_STATIC_LIBRARY)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := zygisk
LOCAL_SRC_FILES := main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/*.c)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/arch/arm/*.c)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/common/*.c)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/third_party/*/*.c)
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/*.c)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/arch/arm64/*.c)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/common/*.c)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/third_party/*/*.c)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/include
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/arch/arm
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/common
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/xdl
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/bsd
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/lss
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/include
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/arch/arm64
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/common
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/xdl
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/bsd
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/lss
endif

ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_STATIC_LIBRARIES := dobby
endif

ifeq ($(TARGET_ARCH_ABI),x86_64)
    LOCAL_STATIC_LIBRARIES := dobby
endif

LOCAL_LDLIBS := -llog -lstdc++
include $(BUILD_SHARED_LIBRARY)