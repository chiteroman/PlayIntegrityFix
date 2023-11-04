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
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

int sh_safe_init(void);
uintptr_t *sh_safe_get_orig_addr_addr(uintptr_t target_addr);

void *sh_safe_pthread_getspecific(pthread_key_t key);
int sh_safe_pthread_setspecific(pthread_key_t key, const void *value);
void sh_safe_abort(void);

void *sh_safe_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int sh_safe_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5);
