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

#include "sh_enter.h"

#include <stdint.h>

#include "sh_trampo.h"

#define SH_ENTER_PAGE_NAME "shadowhook-enter"
#define SH_ENTER_SZ        256
#define SH_ENTER_DELAY_SEC 10

static sh_trampo_mgr_t sh_enter_trampo_mgr;

int sh_enter_init(void) {
  sh_trampo_init_mgr(&sh_enter_trampo_mgr, SH_ENTER_PAGE_NAME, SH_ENTER_SZ, SH_ENTER_DELAY_SEC);
  return 0;
}

uintptr_t sh_enter_alloc(void) {
  return sh_trampo_alloc(&sh_enter_trampo_mgr, 0, 0, 0);
}

void sh_enter_free(uintptr_t enter) {
  sh_trampo_free(&sh_enter_trampo_mgr, enter);
}
