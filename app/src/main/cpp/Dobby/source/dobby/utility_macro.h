#pragma once

// offset of struct member
#define OFFSETOF(TYPE, ELEMENT) ((size_t) & (((TYPE *)0)->ELEMENT))

// assert
#define ASSERT(X)

// left/right shift
#define LeftShift(a, b, c) ((a & ((1 << b) - 1)) << c)
#define RightShift(a, b, c) ((a >> c) & ((1 << b) - 1))

// align
#ifndef ALIGN
#define ALIGN ALIGN_FLOOR
#endif
#define ALIGN_FLOOR(address, range) ((uintptr_t)address & ~((uintptr_t)range - 1))
#define ALIGN_CEIL(address, range) (((uintptr_t)address + (uintptr_t)range - 1) & ~((uintptr_t)range - 1))

// borrow from gdb, refer: binutils-gdb/gdb/arch/arm.h
#define submask(x) ((1L << ((x) + 1)) - 1)
#define bits(obj, st, fn) (((obj) >> (st)) & submask((fn) - (st)))
#define bit(obj, st) (((obj) >> (st)) & 1)
#define sbits(obj, st, fn) ((long)(bits(obj, st, fn) | ((long)bit(obj, fn) * ~submask(fn - st))))

// make it easy
#define set_bit(obj, st, bit) obj = (((~(1 << st)) & obj) | (bit << st))
#define set_bits(obj, st, fn, bits) obj = (((~(submask(fn - st) << st)) & obj) | (bits << st))

// definition to expand macro then apply to pragma message
// #pragma message(VAR_NAME_VALUE(HOST_OS_IOS))
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "=" VALUE(var)

// format print
#ifdef __LP64__
#define __PRI_64_prefix "l"
#define __PRI_PTR_prefix "l"
#else
#define __PRI_64_prefix "ll"
#define __PRI_PTR_prefix
#endif
#define PRIxPTR __PRI_PTR_prefix "x" /* uintptr_t */

// deprecated declared
#if defined(__GNUC__) || defined(__clang__)
#define DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define DEPRECATED __declspec(deprecated)
#else
#pragma message("WARNING: You need to implement DEPRECATED for this compiler")
#define DEPRECATED
#endif

// export method
#if defined(_WIN32)
#define PUBLIC
#else
#define PUBLIC __attribute__((visibility("default")))
#define INTERNAL __attribute__((visibility("internal")))
#endif