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

// Created by Tianzhou Shen (shentianzhou@bytedance.com) on 2020-06-02.

#pragma once

#include <stdbool.h>

#include "bytehook.h"

typedef void (*bh_dl_monitor_post_dlopen_t)(void *arg);
void bh_dl_monitor_set_post_dlopen(bh_dl_monitor_post_dlopen_t cb, void *cb_arg);

typedef void (*bh_dl_monitor_post_dlclose_t)(bool sync_refresh, void *arg);
void bh_dl_monitor_set_post_dlclose(bh_dl_monitor_post_dlclose_t cb, void *cb_arg);

void bh_dl_monitor_add_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data);
void bh_dl_monitor_del_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data);

bool bh_dl_monitor_is_initing(void);
int bh_dl_monitor_init(void);
void bh_dl_monitor_uninit(void);

void bh_dl_monitor_dlclose_rdlock(void);
void bh_dl_monitor_dlclose_unlock(void);
