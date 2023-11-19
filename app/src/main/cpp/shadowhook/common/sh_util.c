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

#include "sh_util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "sh_log.h"
#include "sh_sig.h"
#include "shadowhook.h"

int sh_util_mprotect(uintptr_t addr, size_t len, int prot) {
  uintptr_t start = SH_UTIL_PAGE_START(addr);
  uintptr_t end = SH_UTIL_PAGE_END(addr + len - 1);

  return mprotect((void *)start, end - start, prot);
}

void sh_util_clear_cache(uintptr_t addr, size_t len) {
  __builtin___clear_cache((char *)addr, (char *)(addr + len));
}

bool sh_util_is_thumb32(uintptr_t target_addr) {
  uint16_t opcode = *((uint16_t *)target_addr);
  int tmp = opcode >> 11u;
  return (tmp == 0x1d) || (tmp == 0x1e) || (tmp == 0x1f);
}

static uint32_t sh_util_ror(uint32_t val, uint32_t n, uint32_t shift) {
  uint32_t m = shift % n;
  return (val >> m) | (val << (n - m));
}

uint32_t sh_util_arm_expand_imm(uint32_t opcode) {
  uint32_t imm = SH_UTIL_GET_BITS_32(opcode, 7, 0);
  uint32_t amt = 2 * SH_UTIL_GET_BITS_32(opcode, 11, 8);

  return amt == 0 ? imm : sh_util_ror(imm, 32, amt);
}

int sh_util_write_inst(uintptr_t target_addr, void *inst, size_t inst_len) {
  if (0 != sh_util_mprotect(target_addr, inst_len, PROT_READ | PROT_WRITE | PROT_EXEC))
    return SHADOWHOOK_ERRNO_MPROT;

  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    if ((4 == inst_len) && (0 == target_addr % 4))
      __atomic_store_n((uint32_t *)target_addr, *((uint32_t *)inst), __ATOMIC_SEQ_CST);
    else if ((8 == inst_len) && (0 == target_addr % 8))
      __atomic_store_n((uint64_t *)target_addr, *((uint64_t *)inst), __ATOMIC_SEQ_CST);
#ifdef __LP64__
    else if ((16 == inst_len) && (0 == target_addr % 16))
      __atomic_store_n((__int128 *)target_addr, *((__int128 *)inst), __ATOMIC_SEQ_CST);
#endif
    else
      memcpy((void *)target_addr, inst, inst_len);

    sh_util_clear_cache(target_addr, inst_len);
  }
  SH_SIG_CATCH() {
    return SHADOWHOOK_ERRNO_WRITE_CRASH;
  }
  SH_SIG_EXIT

  return 0;  // OK
}

static bool sh_util_starts_with(const char *str, const char *start) {
  while (*str && *str == *start) {
    str++;
    start++;
  }

  return '\0' == *start;
}

static int sh_util_get_api_level_from_build_prop(void) {
  char buf[128];
  int api_level = -1;

  FILE *fp = fopen("/system/build.prop", "r");
  if (__predict_false(NULL == fp)) goto end;

  while (fgets(buf, sizeof(buf), fp)) {
    if (__predict_false(sh_util_starts_with(buf, "ro.build.version.sdk="))) {
      api_level = atoi(buf + 21);
      break;
    }
  }
  fclose(fp);

end:
  return (api_level > 0) ? api_level : -1;
}

int sh_util_get_api_level(void) {
  static int xdl_util_api_level = -1;

  if (__predict_false(xdl_util_api_level < 0)) {
    int api_level = android_get_device_api_level();
    if (__predict_false(api_level < 0))
      api_level = sh_util_get_api_level_from_build_prop();  // compatible with unusual models
    if (__predict_false(api_level < __ANDROID_API_J__)) api_level = __ANDROID_API_J__;

    __atomic_store_n(&xdl_util_api_level, api_level, __ATOMIC_SEQ_CST);
  }

  return xdl_util_api_level;
}

int sh_util_write(int fd, const char *buf, size_t buf_len) {
  if (fd < 0) return -1;

  const char *ptr = buf;
  size_t nleft = buf_len;

  while (nleft > 0) {
    errno = 0;
    ssize_t nwritten = write(fd, ptr, nleft);
    if (nwritten <= 0) {
      if (nwritten < 0 && errno == EINTR)
        nwritten = 0;  // call write() again
      else
        return -1;  // error
    }
    nleft -= (size_t)nwritten;
    ptr += nwritten;
  }

  return 0;
}

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
#define SH_UTIL_ISLEAP(year) ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define SH_UTIL_SECS_PER_HOUR        (60 * 60)
#define SH_UTIL_SECS_PER_DAY         (SH_UTIL_SECS_PER_HOUR * 24)
#define SH_UTIL_DIV(a, b)            ((a) / (b) - ((a) % (b) < 0))
#define SH_UTIL_LEAPS_THRU_END_OF(y) (SH_UTIL_DIV(y, 4) - SH_UTIL_DIV(y, 100) + SH_UTIL_DIV(y, 400))

static const unsigned short int sh_util_mon_yday[2][13] = {
    /* Normal years.  */
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    /* Leap years.  */
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}};

/* Compute the `struct tm' representation of *T,
   offset GMTOFF seconds east of UTC,
   and store year, yday, mon, mday, wday, hour, min, sec into *RESULT.
   Return RESULT if successful.  */
struct tm *sh_util_localtime_r(const time_t *timep, long gmtoff, struct tm *result) {
  time_t days, rem, y;
  const unsigned short int *ip;

  if (NULL == result) return NULL;

  result->tm_gmtoff = gmtoff;

  days = ((*timep) / SH_UTIL_SECS_PER_DAY);
  rem = ((*timep) % SH_UTIL_SECS_PER_DAY);
  rem += gmtoff;
  while (rem < 0) {
    rem += SH_UTIL_SECS_PER_DAY;
    --days;
  }
  while (rem >= SH_UTIL_SECS_PER_DAY) {
    rem -= SH_UTIL_SECS_PER_DAY;
    ++days;
  }
  result->tm_hour = (int)(rem / SH_UTIL_SECS_PER_HOUR);
  rem %= SH_UTIL_SECS_PER_HOUR;
  result->tm_min = (int)(rem / 60L);
  result->tm_sec = (int)(rem % 60L);
  /* January 1, 1970 was a Thursday.  */
  result->tm_wday = (int)(4 + days) % 7;
  if (result->tm_wday < 0) result->tm_wday += 7;
  y = 1970;

  while (days < 0 || days >= (SH_UTIL_ISLEAP(y) ? 366 : 365)) {
    /* Guess a corrected year, assuming 365 days per year.  */
    time_t yg = y + days / 365 - (days % 365 < 0);

    /* Adjust DAYS and Y to match the guessed year.  */
    days -= ((yg - y) * 365 + SH_UTIL_LEAPS_THRU_END_OF(yg - 1) - SH_UTIL_LEAPS_THRU_END_OF(y - 1));

    y = yg;
  }
  result->tm_year = (int)(y - 1900);
  if (result->tm_year != y - 1900) {
    /* The year cannot be represented due to overflow.  */
    errno = EOVERFLOW;
    return NULL;
  }
  result->tm_yday = (int)days;
  ip = sh_util_mon_yday[SH_UTIL_ISLEAP(y)];
  for (y = 11; days < (long int)ip[y]; --y) continue;
  days -= ip[y];
  result->tm_mon = (int)y;
  result->tm_mday = (int)(days + 1);
  return result;
}

static unsigned sh_util_parse_decimal(const char *format, int *ppos) {
  const char *p = format + *ppos;
  unsigned result = 0;
  for (;;) {
    int ch = *p;
    unsigned d = (unsigned)(ch - '0');
    if (d >= 10U) {
      break;
    }
    result = result * 10 + d;
    p++;
  }
  *ppos = (int)(p - format);
  return result;
}

static void sh_util_format_unsigned(char *buf, size_t buf_size, uint64_t value, int base, int caps) {
  char *p = buf;
  char *end = buf + buf_size - 1;

  // Generate digit string in reverse order.
  while (value) {
    unsigned d = (unsigned)(value % (uint64_t)base);
    value /= (uint64_t)base;
    if (p != end) {
      char ch;
      if (d < 10) {
        ch = '0' + (char)d;
      } else {
        ch = (caps ? 'A' : 'a') + (char)(d - 10);
      }
      *p++ = ch;
    }
  }

  // Special case for 0.
  if (p == buf) {
    if (p != end) {
      *p++ = '0';
    }
  }
  *p = '\0';

  // Reverse digit string in-place.
  size_t length = (size_t)(p - buf);
  for (size_t i = 0, j = length - 1; i < j; ++i, --j) {
    char ch = buf[i];
    buf[i] = buf[j];
    buf[j] = ch;
  }
}

static void sh_util_format_integer(char *buf, size_t buf_size, uint64_t value, char conversion) {
  // Decode the conversion specifier.
  int is_signed = (conversion == 'd' || conversion == 'i' || conversion == 'o');
  int base = 10;
  if (conversion == 'x' || conversion == 'X') {
    base = 16;
  } else if (conversion == 'o') {
    base = 8;
  }
  int caps = (conversion == 'X');
  if (is_signed && (int64_t)(value) < 0) {
    buf[0] = '-';
    buf += 1;
    buf_size -= 1;
    value = (uint64_t)(-(int64_t)(value));
  }
  sh_util_format_unsigned(buf, buf_size, value, base, caps);
}

// format stream
typedef struct {
  size_t total;
  char *pos;
  size_t avail;
} sh_util_stream_t;

static void sh_util_stream_init(sh_util_stream_t *self, char *buffer, size_t buffer_size) {
  self->total = 0;
  self->pos = buffer;
  self->avail = buffer_size;

  if (self->avail > 0) self->pos[0] = '\0';
}

static size_t sh_util_stream_total(sh_util_stream_t *self) {
  return self->total;
}

static void sh_util_stream_send(sh_util_stream_t *self, const char *data, int len) {
  if (len < 0) {
    len = (int)strlen(data);
  }
  self->total += (size_t)len;

  if (self->avail <= 1) {
    // no space to put anything else
    return;
  }

  if ((size_t)len >= self->avail) {
    len = (int)(self->avail - 1);
  }

  memcpy(self->pos, data, (size_t)len);
  self->pos += len;
  self->pos[0] = '\0';
  self->avail -= (size_t)len;
}

static void sh_util_stream_send_repeat(sh_util_stream_t *self, char ch, int count) {
  char pad[8];
  memset(pad, ch, sizeof(pad));

  const int pad_size = (int)(sizeof(pad));
  while (count > 0) {
    int avail = count;
    if (avail > pad_size) {
      avail = pad_size;
    }
    sh_util_stream_send(self, pad, avail);
    count -= avail;
  }
}

static void sh_util_stream_vformat(sh_util_stream_t *self, const char *format, va_list args) {
  int nn = 0;

  for (;;) {
    int mm;
    int padZero = 0;
    int padLeft = 0;
    char sign = '\0';
    int width = -1;
    int prec = -1;
    size_t bytelen = sizeof(int);
    int slen;
    char buffer[32];  // temporary buffer used to format numbers
    char c;

    // first, find all characters that are not 0 or '%', then send them to the output directly
    mm = nn;
    do {
      c = format[mm];
      if (c == '\0' || c == '%') break;
      mm++;
    } while (1);
    if (mm > nn) {
      sh_util_stream_send(self, format + nn, mm - nn);
      nn = mm;
    }

    // is this it ? then exit
    if (c == '\0') break;

    // nope, we are at a '%' modifier
    nn++;  // skip it

    // parse flags
    for (;;) {
      c = format[nn++];
      if (c == '\0') {
        // single trailing '%' ?
        c = '%';
        sh_util_stream_send(self, &c, 1);
        return;
      } else if (c == '0') {
        padZero = 1;
        continue;
      } else if (c == '-') {
        padLeft = 1;
        continue;
      } else if (c == ' ' || c == '+') {
        sign = c;
        continue;
      }
      break;
    }

    // parse field width
    if ((c >= '0' && c <= '9')) {
      nn--;
      width = (int)(sh_util_parse_decimal(format, &nn));
      c = format[nn++];
    }

    // parse precision
    if (c == '.') {
      prec = (int)(sh_util_parse_decimal(format, &nn));
      c = format[nn++];
    }

    // length modifier
    switch (c) {
      case 'h':
        bytelen = sizeof(short);
        if (format[nn] == 'h') {
          bytelen = sizeof(char);
          nn += 1;
        }
        c = format[nn++];
        break;
      case 'l':
        bytelen = sizeof(long);
        if (format[nn] == 'l') {
          bytelen = sizeof(long long);
          nn += 1;
        }
        c = format[nn++];
        break;
      case 'z':
        bytelen = sizeof(size_t);
        c = format[nn++];
        break;
      case 't':
        bytelen = sizeof(ptrdiff_t);
        c = format[nn++];
        break;
      default:;
    }

    // conversion specifier
    const char *str = buffer;
    if (c == 's') {
      // string
      str = va_arg(args, const char *);
      if (str == NULL) {
        str = "(null)";
      }
    } else if (c == 'c') {
      // character
      // NOTE: char is promoted to int when passed through the stack
      buffer[0] = (char)(va_arg(args, int));
      buffer[1] = '\0';
    } else if (c == 'p') {
      uint64_t value = (uintptr_t)(va_arg(args, void *));
      buffer[0] = '0';
      buffer[1] = 'x';
      sh_util_format_integer(buffer + 2, sizeof(buffer) - 2, value, 'x');
    } else if (c == 'd' || c == 'i' || c == 'o' || c == 'u' || c == 'x' || c == 'X') {
      // integers - first read value from stack
      uint64_t value;
      int is_signed = (c == 'd' || c == 'i' || c == 'o');
      // NOTE: int8_t and int16_t are promoted to int when passed through the stack
      switch (bytelen) {
        case 1:
          value = (uint8_t)(va_arg(args, int));
          break;
        case 2:
          value = (uint16_t)(va_arg(args, int));
          break;
        case 4:
          value = va_arg(args, uint32_t);
          break;
        case 8:
          value = va_arg(args, uint64_t);
          break;
        default:
          return;  // should not happen
      }
      // sign extension, if needed
      if (is_signed) {
        int shift = (int)(64 - 8 * bytelen);
        value = (uint64_t)(((int64_t)(value << shift)) >> shift);
      }
      // format the number properly into our buffer
      sh_util_format_integer(buffer, sizeof(buffer), value, c);
    } else if (c == '%') {
      buffer[0] = '%';
      buffer[1] = '\0';
    } else {
      //__assert(__FILE__, __LINE__, "conversion specifier unsupported");
      return;
    }

    // if we are here, 'str' points to the content that must be outputted.
    // handle padding and alignment now
    slen = (int)strlen(str);
    if (sign != '\0' || prec != -1) {
      //__assert(__FILE__, __LINE__, "sign/precision unsupported");
      return;
    }
    if (slen < width && !padLeft) {
      char padChar = padZero ? '0' : ' ';
      sh_util_stream_send_repeat(self, padChar, width - slen);
    }
    sh_util_stream_send(self, str, slen);
    if (slen < width && padLeft) {
      char padChar = padZero ? '0' : ' ';
      sh_util_stream_send_repeat(self, padChar, width - slen);
    }
  }
}

size_t sh_util_vsnprintf(char *buffer, size_t buffer_size, const char *format, va_list args) {
  sh_util_stream_t stream;
  sh_util_stream_init(&stream, buffer, buffer_size);
  sh_util_stream_vformat(&stream, format, args);
  return sh_util_stream_total(&stream);
}

size_t sh_util_snprintf(char *buffer, size_t buffer_size, const char *format, ...) {
  va_list args;
  va_start(args, format);
  size_t buffer_len = sh_util_vsnprintf(buffer, buffer_size, format, args);
  va_end(args);
  return buffer_len;
}
