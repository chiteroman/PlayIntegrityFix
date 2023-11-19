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

// Created by caikelun on 2020-10-04.

#include "xdl_util.h"

#include <android/api-level.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool xdl_util_starts_with(const char *str, const char *start) {
  while (*str && *str == *start) {
    str++;
    start++;
  }

  return '\0' == *start;
}

bool xdl_util_ends_with(const char *str, const char *ending) {
  size_t str_len = strlen(str);
  size_t ending_len = strlen(ending);

  if (ending_len > str_len) return false;

  return 0 == strcmp(str + (str_len - ending_len), ending);
}

size_t xdl_util_trim_ending(char *start) {
  char *end = start + strlen(start);
  while (start < end && isspace((int)(*(end - 1)))) {
    end--;
    *end = '\0';
  }
  return (size_t)(end - start);
}

static int xdl_util_get_api_level_from_build_prop(void) {
  char buf[128];
  int api_level = -1;

  FILE *fp = fopen("/system/build.prop", "r");
  if (NULL == fp) goto end;

  while (fgets(buf, sizeof(buf), fp)) {
    if (xdl_util_starts_with(buf, "ro.build.version.sdk=")) {
      api_level = atoi(buf + 21);
      break;
    }
  }
  fclose(fp);

end:
  return (api_level > 0) ? api_level : -1;
}

int xdl_util_get_api_level(void) {
  static int xdl_util_api_level = -1;

  if (xdl_util_api_level < 0) {
    int api_level = android_get_device_api_level();
    if (api_level < 0)
      api_level = xdl_util_get_api_level_from_build_prop();  // compatible with unusual models
    if (api_level < __ANDROID_API_J__) api_level = __ANDROID_API_J__;

    __atomic_store_n(&xdl_util_api_level, api_level, __ATOMIC_SEQ_CST);
  }

  return xdl_util_api_level;
}
