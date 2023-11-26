// Copyright (c) 2020-2023 HexHacking Team
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

// Created by caikelun on 2020-11-08.

#include "xdl_lzma.h"

#include <ctype.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xdl.h"
#include "xdl_util.h"

// LZMA library pathname & symbol names
#ifndef __LP64__
#define XDL_LZMA_PATHNAME "/system/lib/liblzma.so"
#else
#define XDL_LZMA_PATHNAME "/system/lib64/liblzma.so"
#endif
#define XDL_LZMA_SYM_CRCGEN     "CrcGenerateTable"
#define XDL_LZMA_SYM_CRC64GEN   "Crc64GenerateTable"
#define XDL_LZMA_SYM_CONSTRUCT  "XzUnpacker_Construct"
#define XDL_LZMA_SYM_ISFINISHED "XzUnpacker_IsStreamWasFinished"
#define XDL_LZMA_SYM_FREE       "XzUnpacker_Free"
#define XDL_LZMA_SYM_CODE       "XzUnpacker_Code"

// LZMA data type definition
#define SZ_OK 0
typedef struct ISzAlloc ISzAlloc;
typedef const ISzAlloc *ISzAllocPtr;
struct ISzAlloc {
  void *(*Alloc)(ISzAllocPtr p, size_t size);
  void (*Free)(ISzAllocPtr p, void *address); /* address can be 0 */
};
typedef enum {
  CODER_STATUS_NOT_SPECIFIED,      /* use main error code instead */
  CODER_STATUS_FINISHED_WITH_MARK, /* stream was finished with end mark. */
  CODER_STATUS_NOT_FINISHED,       /* stream was not finished */
  CODER_STATUS_NEEDS_MORE_INPUT    /* you must provide more input bytes */
} ECoderStatus;
typedef enum {
  CODER_FINISH_ANY, /* finish at any point */
  CODER_FINISH_END  /* block must be finished at the end */
} ECoderFinishMode;

// LZMA function type definition
typedef void (*xdl_lzma_crcgen_t)(void);
typedef void (*xdl_lzma_crc64gen_t)(void);
typedef void (*xdl_lzma_construct_t)(void *, ISzAllocPtr);
typedef int (*xdl_lzma_isfinished_t)(const void *);
typedef void (*xdl_lzma_free_t)(void *);
typedef int (*xdl_lzma_code_t)(void *, uint8_t *, size_t *, const uint8_t *, size_t *, ECoderFinishMode,
                               ECoderStatus *);
typedef int (*xdl_lzma_code_q_t)(void *, uint8_t *, size_t *, const uint8_t *, size_t *, int,
                                 ECoderFinishMode, ECoderStatus *);

// LZMA function pointor
static xdl_lzma_construct_t xdl_lzma_construct = NULL;
static xdl_lzma_isfinished_t xdl_lzma_isfinished = NULL;
static xdl_lzma_free_t xdl_lzma_free = NULL;
static void *xdl_lzma_code = NULL;

// LZMA init
static void xdl_lzma_init() {
  void *lzma = xdl_open(XDL_LZMA_PATHNAME, XDL_TRY_FORCE_LOAD);
  if (NULL == lzma) return;

  xdl_lzma_crcgen_t crcgen = NULL;
  xdl_lzma_crc64gen_t crc64gen = NULL;
  if (NULL == (crcgen = (xdl_lzma_crcgen_t)xdl_sym(lzma, XDL_LZMA_SYM_CRCGEN, NULL))) goto end;
  if (NULL == (crc64gen = (xdl_lzma_crc64gen_t)xdl_sym(lzma, XDL_LZMA_SYM_CRC64GEN, NULL))) goto end;
  if (NULL == (xdl_lzma_construct = (xdl_lzma_construct_t)xdl_sym(lzma, XDL_LZMA_SYM_CONSTRUCT, NULL)))
    goto end;
  if (NULL == (xdl_lzma_isfinished = (xdl_lzma_isfinished_t)xdl_sym(lzma, XDL_LZMA_SYM_ISFINISHED, NULL)))
    goto end;
  if (NULL == (xdl_lzma_free = (xdl_lzma_free_t)xdl_sym(lzma, XDL_LZMA_SYM_FREE, NULL))) goto end;
  if (NULL == (xdl_lzma_code = xdl_sym(lzma, XDL_LZMA_SYM_CODE, NULL))) goto end;
  crcgen();
  crc64gen();

end:
  xdl_close(lzma);
}

// LZMA internal alloc / free
static void *xdl_lzma_internal_alloc(ISzAllocPtr p, size_t size) {
  (void)p;
  return malloc(size);
}
static void xdl_lzma_internal_free(ISzAllocPtr p, void *address) {
  (void)p;
  free(address);
}

int xdl_lzma_decompress(uint8_t *src, size_t src_size, uint8_t **dst, size_t *dst_size) {
  size_t src_offset = 0;
  size_t dst_offset = 0;
  size_t src_remaining;
  size_t dst_remaining;
  ISzAlloc alloc = {.Alloc = xdl_lzma_internal_alloc, .Free = xdl_lzma_internal_free};
  long long state[4096 / sizeof(long long)];  // must be enough, 8-bit aligned
  ECoderStatus status;
  int api_level = xdl_util_get_api_level();

  // init and check
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  static bool inited = false;
  if (!inited) {
    pthread_mutex_lock(&lock);
    if (!inited) {
      xdl_lzma_init();
      inited = true;
    }
    pthread_mutex_unlock(&lock);
  }
  if (NULL == xdl_lzma_code) return -1;

  xdl_lzma_construct(&state, &alloc);

  *dst_size = 2 * src_size;
  *dst = NULL;
  do {
    *dst_size *= 2;
    if (NULL == (*dst = realloc(*dst, *dst_size))) {
      xdl_lzma_free(&state);
      return -1;
    }

    src_remaining = src_size - src_offset;
    dst_remaining = *dst_size - dst_offset;

    int result;
    if (api_level >= __ANDROID_API_Q__) {
      xdl_lzma_code_q_t lzma_code_q = (xdl_lzma_code_q_t)xdl_lzma_code;
      result = lzma_code_q(&state, *dst + dst_offset, &dst_remaining, src + src_offset, &src_remaining, 1,
                           CODER_FINISH_ANY, &status);
    } else {
      xdl_lzma_code_t lzma_code = (xdl_lzma_code_t)xdl_lzma_code;
      result = lzma_code(&state, *dst + dst_offset, &dst_remaining, src + src_offset, &src_remaining,
                         CODER_FINISH_ANY, &status);
    }
    if (SZ_OK != result) {
      free(*dst);
      xdl_lzma_free(&state);
      return -1;
    }

    src_offset += src_remaining;
    dst_offset += dst_remaining;
  } while (status == CODER_STATUS_NOT_FINISHED);

  xdl_lzma_free(&state);

  if (!xdl_lzma_isfinished(&state)) {
    free(*dst);
    return -1;
  }

  *dst_size = dst_offset;
  *dst = realloc(*dst, *dst_size);
  return 0;
}
