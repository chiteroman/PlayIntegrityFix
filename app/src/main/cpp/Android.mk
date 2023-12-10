LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := zygisk
LOCAL_SRC_FILES := $(LOCAL_PATH)/main.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/common/*.c)
LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/third_party/xdl/*.c)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook
LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/common
LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/bsd
LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/lss
LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/third_party/xdl

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/arch/arm/*.c)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/arch/arm
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
    LOCAL_SRC_FILES += $(wildcard $(LOCAL_PATH)/shadowhook/arch/arm64/*.c)
    LOCAL_C_INCLUDES += $(LOCAL_PATH)/shadowhook/arch/arm64
endif

LOCAL_STATIC_LIBRARIES := libcxx
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/libcxx/Android.mk