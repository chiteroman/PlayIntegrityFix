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

#include "sh_log.h"

#include <android/log.h>
#include <stdbool.h>

#include "sh_config.h"

android_LogPriority sh_log_priority =
#ifdef SH_CONFIG_DEBUG
    ANDROID_LOG_INFO
#else
    ANDROID_LOG_SILENT
#endif
    ;

bool sh_log_get_debuggable(void) {
  return sh_log_priority <= ANDROID_LOG_INFO;
}

void sh_log_set_debuggable(bool debuggable) {
#ifdef SH_CONFIG_DEBUG
  (void)debuggable;
  sh_log_priority = ANDROID_LOG_INFO;
#else
  if (__predict_false(debuggable))
    sh_log_priority = ANDROID_LOG_INFO;
  else
    sh_log_priority = ANDROID_LOG_SILENT;
#endif
}
