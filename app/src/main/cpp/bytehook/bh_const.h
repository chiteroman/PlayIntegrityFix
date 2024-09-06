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

#ifndef __LP64__
#define BH_CONST_PATHNAME_LINKER      "/system/bin/linker"
#define BH_CONST_BASENAME_LINKER      "linker"
#define BH_CONST_BASENAME_APP_PROCESS "app_process32"
#else
#define BH_CONST_PATHNAME_LINKER      "/system/bin/linker64"
#define BH_CONST_BASENAME_LINKER      "linker64"
#define BH_CONST_BASENAME_APP_PROCESS "app_process64"
#endif

#define BH_CONST_BASENAME_DL       "libdl.so"
#define BH_CONST_BASENAME_BYTEHOOK "libbytehook.so"

#define BH_CONST_SYM_DLCLOSE                   "dlclose"
#define BH_CONST_SYM_LOADER_DLCLOSE            "__loader_dlclose"
#define BH_CONST_SYM_DLOPEN                    "dlopen"
#define BH_CONST_SYM_ANDROID_DLOPEN_EXT        "android_dlopen_ext"
#define BH_CONST_SYM_DLOPEN_EXT                "__dl__ZL10dlopen_extPKciPK17android_dlextinfoPv"
#define BH_CONST_SYM_LOADER_DLOPEN             "__loader_dlopen"
#define BH_CONST_SYM_LOADER_ANDROID_DLOPEN_EXT "__loader_android_dlopen_ext"
#define BH_CONST_SYM_DO_DLOPEN                 "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv"
#define BH_CONST_SYM_G_DL_MUTEX                "__dl__ZL10g_dl_mutex"
#define BH_CONST_SYM_G_DL_MUTEX_U_QPR2         "__dl_g_dl_mutex"
#define BH_CONST_SYM_LINKER_GET_ERROR_BUFFER   "__dl__Z23linker_get_error_bufferv"
#define BH_CONST_SYM_BIONIC_FORMAT_DLERROR     "__dl__ZL23__bionic_format_dlerrorPKcS0_"
#define BH_CONST_SYM_CFI_SLOWPATH              "__cfi_slowpath"
#define BH_CONST_SYM_CFI_SLOWPATH_DIAG         "__cfi_slowpath_diag"
