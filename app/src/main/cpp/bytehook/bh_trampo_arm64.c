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

#if defined(__aarch64__)

#include "bh_trampo.h"

__attribute__((naked)) void bh_trampo_template() {
  __asm__(
      // Save caller-saved registers
      "sub   sp, sp, #0xd0            \n"
      "stp   q0, q1, [sp, #0xb0]      \n"
      "stp   q2, q3, [sp, #0x90]      \n"
      "stp   q4, q5, [sp, #0x70]      \n"
      "stp   q6, q7, [sp, #0x50]      \n"
      "stp   x0, x1, [sp, #0x40]      \n"
      "stp   x2, x3, [sp, #0x30]      \n"
      "stp   x4, x5, [sp, #0x20]      \n"
      "stp   x6, x7, [sp, #0x10]      \n"
      "stp   x8, lr, [sp]             \n"

      // Call bh_trampo_push_stack()
      "ldr   x0, .L_hook_ptr          \n"
      "mov   x1, lr                   \n"
      "ldr   x16, .L_push_stack       \n"
      "blr   x16                      \n"

      // Save the hook function's address to IP register
      "mov   x16, x0                  \n"

      // Restore caller-saved registers
      "ldp   x8, lr, [sp]             \n"
      "ldp   x6, x7, [sp, #0x10]      \n"
      "ldp   x4, x5, [sp, #0x20]      \n"
      "ldp   x2, x3, [sp, #0x30]      \n"
      "ldp   x0, x1, [sp, #0x40]      \n"
      "ldp   q6, q7, [sp, #0x50]      \n"
      "ldp   q4, q5, [sp, #0x70]      \n"
      "ldp   q2, q3, [sp, #0x90]      \n"
      "ldp   q0, q1, [sp, #0xb0]      \n"
      "add   sp, sp, #0xd0            \n"

      // Call hook function
      "br    x16                      \n"

      "bh_trampo_data:"
      ".global bh_trampo_data;"
      ".L_push_stack:"
      ".quad 0;"
      ".L_hook_ptr:"
      ".quad 0;");
}

#else
typedef int make_iso_happy;
#endif
