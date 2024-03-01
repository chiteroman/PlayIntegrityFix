// Copyright (c) 2021-2022 ByteDance Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by Kelun Cai (caikelun@bytedance.com) on 2021-04-11.

#pragma once
#include <android/log.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

extern android_LogPriority sh_log_priority;

#define SH_LOG_TAG "shadowhook_tag"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

// Notice:
// We don't use ANDROID_LOG_DEBUG, because some Android devices will filter out ANDROID_LOG_DEBUG.
#ifdef SH_CONFIG_DEBUG
#define SH_LOG_DEBUG(fmt, ...)                                               \
  do {                                                                       \
    if (sh_log_priority <= ANDROID_LOG_INFO)                                 \
      __android_log_print(ANDROID_LOG_INFO, SH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)
#else
#define SH_LOG_DEBUG(fmt, ...)
#endif

#define SH_LOG_INFO(fmt, ...)                                                \
  do {                                                                       \
    if (__predict_false(sh_log_priority <= ANDROID_LOG_INFO))                \
      __android_log_print(ANDROID_LOG_INFO, SH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)
#define SH_LOG_WARN(fmt, ...)                                                \
  do {                                                                       \
    if (__predict_false(sh_log_priority <= ANDROID_LOG_WARN))                \
      __android_log_print(ANDROID_LOG_WARN, SH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)
#define SH_LOG_ERROR(fmt, ...)                                                \
  do {                                                                        \
    if (__predict_false(sh_log_priority <= ANDROID_LOG_ERROR))                \
      __android_log_print(ANDROID_LOG_ERROR, SH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)
#define SH_LOG_ALWAYS_SHOW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, SH_LOG_TAG, fmt, ##__VA_ARGS__)

#pragma clang diagnostic pop

bool sh_log_get_debuggable(void);
void sh_log_set_debuggable(bool debuggable);
