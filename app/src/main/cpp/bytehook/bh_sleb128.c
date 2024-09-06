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

#include "bh_sleb128.h"

#include <stddef.h>
#include <stdint.h>

void bh_sleb128_decoder_init(bh_sleb128_decoder_t *self, uint8_t *data, size_t data_sz) {
  self->cur = data;
  self->end = data + data_sz;
}

int bh_sleb128_decoder_next(bh_sleb128_decoder_t *self, size_t *ret) {
  size_t value = 0;
  static const size_t size = 8 * sizeof(value);
  size_t shift = 0;
  uint8_t byte;

  do {
    if (self->cur >= self->end) return -1;

    byte = *(self->cur)++;
    value |= ((size_t)(byte & 127) << shift);
    shift += 7;
  } while (byte & 128);

  if (shift < size && (byte & 64)) {
    value |= -((size_t)(1) << shift);
  }

  *ret = value;
  return 0;
}
