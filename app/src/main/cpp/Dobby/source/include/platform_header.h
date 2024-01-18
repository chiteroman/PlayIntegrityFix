#pragma once

#if defined(__APPLE__) && __arm64e__
#if __has_feature(ptrauth_calls)
#include <ptrauth.h>
#endif
#endif

#if defined(BUILDING_KERNEL)
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <math.h>
#include <machine/limits.h>
#else
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <syslog.h>
#endif
#endif

#if defined(BUILDING_KERNEL)
#include "kernel_mode_header.h"
#endif

#if defined(BUILDING_KERNEL)
#define abs(a) ((a) < 0 ? -(a) : (a))
#define llabs(a) (((long long)a) < 0 ? -((long long)a) : ((long long)a))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#ifdef __cplusplus
#define abs(a) ((a) < 0 ? -(a) : (a))
#endif
#else
#ifdef __cplusplus
#include <type_traits>
#include <vector>
#include <map>
#include "TINYSTL/vector.h"
#include "TINYSTL/unordered_map.h"
#endif
#endif