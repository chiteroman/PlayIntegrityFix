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

// Created by Li Zhang (zhangli.foxleezh@bytedance.com) on 2020-06-21.

#if defined(__arm__)

#include "bh_trampo.h"

__attribute__((naked)) void bh_trampo_template(void) {
  __asm__(
      // Save caller-saved registers
      "push  { r0 - r3, lr }     \n"

      // Call bh_trampo_push_stack()
      "ldr   r0, .L_hook_ptr     \n"
      "mov   r1, lr              \n"
      "ldr   ip, .L_push_stack   \n"
      "blx   ip                  \n"

      // Save the hook function's address to IP register
      "mov   ip, r0              \n"

      // Restore caller-saved registers
      "pop   { r0 - r3, lr }     \n"

      // Call hook function
      "bx    ip                  \n"

      "bh_trampo_data:"
      ".global bh_trampo_data;"
      ".L_push_stack:"
      ".word 0;"
      ".L_hook_ptr:"
      ".word 0;");
}

#else
typedef int make_iso_happy;
#endif
