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
#include "sh_log.h"

#define SH_ERRNO_SET_RET_ERRNUM(errnum) SH_ERRNO_SET_RET((errnum), (errnum))
#define SH_ERRNO_SET_RET_FAIL(errnum)   SH_ERRNO_SET_RET((errnum), -1)
#define SH_ERRNO_SET_RET_NULL(errnum)   SH_ERRNO_SET_RET((errnum), NULL)
#define SH_ERRNO_SET_RET(errnum, ret) \
  do {                                \
    sh_errno_set((errnum));           \
    return (ret);                     \
  } while (0)

int sh_errno_init(void);
void sh_errno_reset(void);
void sh_errno_set(int error_number);
int sh_errno_get(void);
const char *sh_errno_to_errmsg(int error_number);
