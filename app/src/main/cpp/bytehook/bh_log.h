// Copyright (c) 2020-2022 ByteDance, Inc.
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

// Created by Kelun Cai (caikelun@bytedance.com) on 2020-06-02.

#pragma once
#include <android/log.h>
#include <stdarg.h>
#include <stdbool.h>

extern android_LogPriority bh_log_priority;

#define BH_LOG_TAG "bytehook_tag"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define BH_LOG_INFO(fmt, ...)                                                \
  do {                                                                       \
    if (__predict_false(bh_log_priority <= ANDROID_LOG_INFO))                \
      __android_log_print(ANDROID_LOG_INFO, BH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)
#define BH_LOG_WARN(fmt, ...)                                                \
  do {                                                                       \
    if (__predict_false(bh_log_priority <= ANDROID_LOG_WARN))                \
      __android_log_print(ANDROID_LOG_WARN, BH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)
#define BH_LOG_ERROR(fmt, ...)                                                \
  do {                                                                        \
    if (__predict_false(bh_log_priority <= ANDROID_LOG_ERROR))                \
      __android_log_print(ANDROID_LOG_ERROR, BH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)

#define BH_LOG_SHOW(fmt, ...)                                              \
  do {                                                                     \
    __android_log_print(ANDROID_LOG_WARN, BH_LOG_TAG, fmt, ##__VA_ARGS__); \
  } while (0)

#pragma clang diagnostic pop

bool bh_log_get_debug(void);
void bh_log_set_debug(bool debug);
