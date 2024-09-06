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
#include <stdbool.h>

#include "bh_elf.h"

typedef struct bh_elf_manager bh_elf_manager_t;

bh_elf_manager_t *bh_elf_manager_create(void);

int bh_elf_manager_add_ignore(bh_elf_manager_t *self, const char *caller_path_name);

typedef void (*bh_elf_manager_post_add_cb_t)(bh_elf_t *elf, void *arg);
void bh_elf_manager_refresh(bh_elf_manager_t *self, bool sync_clean, bh_elf_manager_post_add_cb_t cb,
                            void *cb_arg);

typedef bool (*bh_elf_manager_iterate_cb_t)(bh_elf_t *elf, void *arg);
void bh_elf_manager_iterate(bh_elf_manager_t *self, bh_elf_manager_iterate_cb_t cb, void *cb_arg);

bh_elf_t *bh_elf_manager_find_elf(bh_elf_manager_t *self, const char *pathname);

// similar to dlsym(), but search .dynsym
void *bh_elf_manager_find_export_addr(bh_elf_manager_t *self, const char *pathname, const char *sym_name);
