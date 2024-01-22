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
#include <stdbool.h>
#include <stdint.h>

int sh_hub_init(void);

typedef struct sh_hub sh_hub_t;

sh_hub_t *sh_hub_create(uintptr_t target_addr, uintptr_t *trampo);
void sh_hub_destroy(sh_hub_t *self, bool with_delay);

uintptr_t sh_hub_get_orig_addr(sh_hub_t *self);
uintptr_t *sh_hub_get_orig_addr_addr(sh_hub_t *self);

int sh_hub_add_proxy(sh_hub_t *self, uintptr_t func);
int sh_hub_del_proxy(sh_hub_t *self, uintptr_t func, bool *have_enabled_proxy);

void *sh_hub_get_prev_func(void *func);
void sh_hub_pop_stack(void *return_address);
void sh_hub_allow_reentrant(void *return_address);
void sh_hub_disallow_reentrant(void *return_address);
void *sh_hub_get_return_address(void);
