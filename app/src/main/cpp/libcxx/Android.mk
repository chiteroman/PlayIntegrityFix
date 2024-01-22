# This file is dual licensed under the MIT and the University of Illinois Open
# Source Licenses. See LICENSE.TXT for details.

LOCAL_PATH := $(call my-dir)

# libcxx defines
libcxx_includes := $(LOCAL_PATH)/include $(LOCAL_PATH)/src
libcxx_export_includes := $(libcxx_includes)
libcxx_sources := \
    algorithm.cpp \
    any.cpp \
    atomic.cpp \
    barrier.cpp \
    bind.cpp \
    charconv.cpp \
    chrono.cpp \
    condition_variable.cpp \
    condition_variable_destructor.cpp \
    debug.cpp \
    exception.cpp \
    filesystem/directory_iterator.cpp \
    filesystem/int128_builtins.cpp \
    filesystem/operations.cpp \
    functional.cpp \
    future.cpp \
    hash.cpp \
    ios.cpp \
    ios.instantiations.cpp \
    iostream.cpp \
    legacy_debug_handler.cpp \
    legacy_pointer_safety.cpp \
    locale.cpp \
    memory.cpp \
    memory_resource.cpp \
    mutex.cpp \
    mutex_destructor.cpp \
    new.cpp \
    optional.cpp \
    random_shuffle.cpp \
    random.cpp \
    regex.cpp \
    shared_mutex.cpp \
    stdexcept.cpp \
    string.cpp \
    strstream.cpp \
    system_error.cpp \
    thread.cpp \
    typeinfo.cpp \
    utility.cpp \
    valarray.cpp \
    variant.cpp \
    vector.cpp \
    verbose_abort.cpp \

libcxx_sources := $(libcxx_sources:%=src/%)

libcxx_export_cxxflags :=

libcxx_cxxflags := \
    -std=c++20 \
    -fvisibility-global-new-delete-hidden \
    -fvisibility=hidden -fvisibility-inlines-hidden \
    -DLIBCXX_BUILDING_LIBCXXABI \
    -D_LIBCPP_NO_EXCEPTIONS \
    -D_LIBCPP_NO_RTTI \
    -D_LIBCPP_BUILDING_LIBRARY \
    -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS \
    -D__STDC_FORMAT_MACROS \
    $(libcxx_export_cxxflags) \


libcxx_ldflags :=
libcxx_export_ldflags :=

# libcxxabi defines
libcxxabi_src_files := \
    abort_message.cpp \
    cxa_aux_runtime.cpp \
    cxa_default_handlers.cpp \
    cxa_exception_storage.cpp \
    cxa_guard.cpp \
    cxa_handlers.cpp \
    cxa_noexception.cpp \
    cxa_thread_atexit.cpp \
    cxa_vector.cpp \
    cxa_virtual.cpp \
    stdlib_exception.cpp \
    stdlib_new_delete.cpp \
    stdlib_stdexcept.cpp \
    stdlib_typeinfo.cpp \

libcxxabi_src_files := $(libcxxabi_src_files:%=src/abi/%)

libcxxabi_includes := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/include/abi \

libcxxabi_cflags := -D__STDC_FORMAT_MACROS
libcxxabi_cppflags := \
    -D_LIBCXXABI_NO_EXCEPTIONS \
    -Wno-macro-redefined \
    -Wno-unknown-attributes \
    -DHAS_THREAD_LOCAL

include $(CLEAR_VARS)
LOCAL_MODULE := libcxx
LOCAL_SRC_FILES := $(libcxx_sources) $(libcxxabi_src_files)
LOCAL_C_INCLUDES := $(libcxx_includes) $(libcxxabi_includes)
LOCAL_CPPFLAGS := $(libcxx_cxxflags) $(libcxxabi_cppflags) -ffunction-sections -fdata-sections
LOCAL_EXPORT_C_INCLUDES := $(libcxx_export_includes)
LOCAL_EXPORT_CPPFLAGS := $(libcxx_export_cxxflags)
LOCAL_EXPORT_LDFLAGS := $(libcxx_export_ldflags)

include $(BUILD_STATIC_LIBRARY)
