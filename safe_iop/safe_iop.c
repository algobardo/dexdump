/* safe_iop
 * Author:: Will Drewry <redpig@dataspill.org>
 * See safe_iop.h for more info.
 *
 * Copyright (c) 2007,2008 Will Drewry <redpig@dataspill.org>
 * Some portions contributed by Google Inc., 2008.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>

#include "safe_iop.h"

/* Read off the type if the first value matches a type prefix
 * and consume characters if successful.
 */
static int _safe_op_read_type(safe_type_t *type, const char **c) {
  if (type == NULL) {
    return 0;
  }
  if (c == NULL || *c == NULL || **c == '\0') {
    return 0;
  }
  /* Extract a type for the operation if there is one */
  if (strchr(SAFE_IOP_TYPE_PREFIXES, **c) != NULL) {
    switch(**c) {
      case 'u':
        if ((*(*c+1) && *(*c+1) == '3') &&
            (*(*c+2) && *(*c+2) == '2')) {
          *type = SAFE_IOP_TYPE_U32;
          *c += 3; /* Advance past type */
        }
        break;
      case 's':
        if ((*(*c+1) && *(*c+1) == '3') &&
            (*(*c+2) && *(*c+2) == '2')) {
          *type = SAFE_IOP_TYPE_S32;
          *c += 3; /* Advance past type */
        }
        break;
      default:
        /* Unknown type */
        return 0;
    }
  }
  return 1;
}

#define _SAFE_IOP_TYPE_CASE(_type, _func) { \
  _type a = va_arg(ap, _type), value = *((_type *) result); \
  if (!baseline) { \
    value = a; \
    a = va_arg(ap, _type); \
    baseline = 1; \
  } \
  if (! _func( (_type *) result, value, a)) \
    return 0; \
}
#define _SAFE_IOP_OP_CASE(u32func, s32func) \
  switch (type) { \
    case SAFE_IOP_TYPE_U32: \
      _SAFE_IOP_TYPE_CASE(u_int32_t, u32func); \
      break; \
    case SAFE_IOP_TYPE_S32: \
      _SAFE_IOP_TYPE_CASE(int32_t, s32func); \
      break; \
    default: \
      return 0; \
  }

int safe_iopf(void *result, const char *const fmt, ...) {
  va_list ap;
  int baseline = 0; /* indicates if the base value is present */

  const char *c = NULL;
  safe_type_t type = SAFE_IOP_TYPE_DEFAULT;
  /* Result should not be NULL */
  if (!result)
    return 0;

  va_start(ap, fmt);
  if (fmt == NULL || fmt[0] == '\0')
    return 0;
  for(c=fmt;(*c);c++) {
    /* Read the type if specified */
    if (!_safe_op_read_type(&type, &c)) {
      return 0;
    }

    /* Process the the operations */
    switch(*c) { /* operation */
      case '+': /* add */
        _SAFE_IOP_OP_CASE(safe_uadd, safe_sadd);
        break;
      case '-': /* sub */
        _SAFE_IOP_OP_CASE(safe_usub, safe_ssub);
        break;
      case '*': /* mul */
        _SAFE_IOP_OP_CASE(safe_umul, safe_smul);
        break;
      case '/': /* div */
        _SAFE_IOP_OP_CASE(safe_udiv, safe_sdiv);
        break;
      case '%': /* mod */
        _SAFE_IOP_OP_CASE(safe_umod, safe_smod);
        break;
      default:
       /* unknown op */
       return 0;
    }
    /* Reset the type */
   type = SAFE_IOP_TYPE_DEFAULT;
  }
  /* Success! */
  return 1;
}

#ifdef SAFE_IOP_TEST
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>

/* __LP64__ is given by GCC. Without more work, this is bound to GCC. */
#if __LP64__ == 1 || __SIZEOF_LONG__ > __SIZEOF_INT__
#  define SAFE_INT64_MAX 0x7fffffffffffffffL
#  define SAFE_UINT64_MAX 0xffffffffffffffffUL
#  define SAFE_INT64_MIN (-SAFE_INT64_MAX - 1L)
#elif __SIZEOF_LONG__ == __SIZEOF_INT__
#  define SAFE_INT64_MAX 0x7fffffffffffffffLL
#  define SAFE_UINT64_MAX 0xffffffffffffffffULL
#  define SAFE_INT64_MIN (-SAFE_INT64_MAX - 1LL)
#else
#  warning "64-bit support disabled"
#  define SAFE_IOP_NO_64 1
#endif

/* Pull these from GNU's limit.h */
#ifndef LLONG_MAX
#  define LLONG_MAX 9223372036854775807LL
#endif
#ifndef LLONG_MIN
#  define LLONG_MIN (-LLONG_MAX - 1LL)
#endif
#ifndef ULLONG_MAX
#  define ULLONG_MAX 18446744073709551615ULL
#endif

/* Assumes SSIZE_MAX */
#ifndef SSIZE_MIN
#  if SSIZE_MAX == LONG_MAX
#    define SSIZE_MIN LONG_MIN
#  elif SSIZE_MAX == LONG_LONG_MAX
#    define SSIZE_MIN LONG_LONG_MIN
#  else
#    error "SSIZE_MIN is not defined and could not be guessed"
#  endif
#endif

#define EXPECT_FALSE(cmd) ({ \
  printf("%s: EXPECT_FALSE(" #cmd ") => ", __func__); \
  if ((cmd) != 0) { printf(" FAILED\n"); expect_fail++; r = 0; } \
  else { printf(" PASSED\n"); expect_succ++; } \
  expect++; \
  })
#define EXPECT_TRUE(cmd) ({ \
  printf("%s: EXPECT_TRUE(" #cmd ") => ", __func__); \
  if ((cmd) != 1) { printf(" FAILED\n"); expect_fail++; r = 0; } \
  else { printf(" PASSED\n"); expect_succ++; } \
  expect++;  \
  })

static int expect = 0, expect_succ = 0, expect_fail = 0;

/***** ADD *****/
int T_add_s8() {
  int r=1;
  int8_t a, b;
  a=SCHAR_MIN; b=-1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SCHAR_MAX; b=1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SCHAR_MAX; EXPECT_FALSE(safe_inc(&a));
  a=0; EXPECT_TRUE(safe_inc(&a)); EXPECT_TRUE(a==1);
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SCHAR_MIN; b=SCHAR_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SCHAR_MAX/2; b=SCHAR_MAX/2; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_s16() {
  int r=1;
  int16_t a, b;
  a=SHRT_MIN; b=-1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SHRT_MAX; b=1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SHRT_MAX; EXPECT_FALSE(safe_inc(&a));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SHRT_MIN; b=SHRT_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SHRT_MAX/2; b=SHRT_MAX/2; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_s32() {
  int r=1;
  int32_t a, b;
  a=INT_MIN; b=-1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=INT_MAX; b=1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=INT_MAX; EXPECT_FALSE(safe_inc(&a));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=INT_MIN; b=INT_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  a=INT_MAX/2; b=INT_MAX/2; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_s64() {
  int r=1;
  int64_t a, b;
  a=SAFE_INT64_MIN; b=-1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SAFE_INT64_MAX; b=1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SAFE_INT64_MAX; EXPECT_FALSE(safe_inc(&a));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SAFE_INT64_MIN; b=SAFE_INT64_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SAFE_INT64_MAX/2; b=SAFE_INT64_MAX/2; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_long() {
  int r=1;
  long a, b;
  a=LONG_MIN; b=-1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=LONG_MAX; b=1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=LONG_MAX; EXPECT_FALSE(safe_inc(&a));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=LONG_MIN; b=LONG_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  a=LONG_MAX/2; b=LONG_MAX/2; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}
int T_add_longlong() {
  int r=1;
  long long a, b;
  a=LLONG_MIN; b=-1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=LLONG_MAX; b=1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=LLONG_MAX; EXPECT_FALSE(safe_inc(&a));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=LLONG_MIN; b=LLONG_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  a=LLONG_MAX/2; b=LLONG_MAX/2; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}
int T_add_ssizet() {
  int r=1;
  ssize_t a, b;
  a=SSIZE_MIN; b=-1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SSIZE_MAX; b=1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SSIZE_MAX; EXPECT_FALSE(safe_inc(&a));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SSIZE_MIN; b=SSIZE_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SSIZE_MAX/2; b=SSIZE_MAX/2; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_u8() {
  int r=1;
  uint8_t a, b;
  a=1; b=UCHAR_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=UCHAR_MAX; EXPECT_FALSE(safe_inc(&a));
  a=UCHAR_MAX/2; b=a+2; EXPECT_FALSE(safe_add(NULL, a, b));
  a=UCHAR_MAX/2; b=a; EXPECT_TRUE(safe_add(NULL, a, b));
  a=UCHAR_MAX/2; b=a+1; EXPECT_TRUE(safe_add(NULL, a, b));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=0; b=UCHAR_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_u16() {
  int r=1;
  uint16_t a, b;
  a=1; b=USHRT_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=USHRT_MAX; EXPECT_FALSE(safe_inc(&a));
  a=USHRT_MAX/2; b=a+2; EXPECT_FALSE(safe_add(NULL, a, b));
  a=USHRT_MAX/2; b=a; EXPECT_TRUE(safe_add(NULL, a, b));
  a=USHRT_MAX/2; b=a+1; EXPECT_TRUE(safe_add(NULL, a, b));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=0; b=USHRT_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_u32() {
  int r=1;
  uint32_t a, b;
  a=1; b=UINT_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=UINT_MAX; EXPECT_FALSE(safe_inc(&a));
  a=UINT_MAX/2; b=a+2; EXPECT_FALSE(safe_add(NULL, a, b));
  a=UINT_MAX/2; b=a; EXPECT_TRUE(safe_add(NULL, a, b));
  a=UINT_MAX/2; b=a+1; EXPECT_TRUE(safe_add(NULL, a, b));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=0; b=UINT_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_u64() {
  int r=1;
  uint64_t a, b;
  a=1; b=SAFE_UINT64_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SAFE_UINT64_MAX; EXPECT_FALSE(safe_inc(&a));
  a=SAFE_UINT64_MAX/2; b=a+2; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SAFE_UINT64_MAX/2; b=a; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SAFE_UINT64_MAX/2; b=a+1; EXPECT_TRUE(safe_add(NULL, a, b));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=0; b=SAFE_UINT64_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_ulong() {
  int r=1;
  unsigned long a, b;
  a=1; b=ULONG_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=ULONG_MAX; EXPECT_FALSE(safe_inc(&a));
  a=ULONG_MAX/2; b=a+2; EXPECT_FALSE(safe_add(NULL, a, b));
  a=ULONG_MAX/2; b=a; EXPECT_TRUE(safe_add(NULL, a, b));
  a=ULONG_MAX/2; b=a+1; EXPECT_TRUE(safe_add(NULL, a, b));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=0; b=ULONG_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_ulonglong() {
  int r=1;
  unsigned long long a, b;
  a=1; b=ULLONG_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=ULLONG_MAX; EXPECT_FALSE(safe_inc(&a));
  a=ULLONG_MAX/2; b=a+2; EXPECT_FALSE(safe_add(NULL, a, b));
  a=ULLONG_MAX/2; b=a; EXPECT_TRUE(safe_add(NULL, a, b));
  a=ULLONG_MAX/2; b=a+1; EXPECT_TRUE(safe_add(NULL, a, b));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=0; b=ULLONG_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_sizet() {
  int r=1;
  size_t a, b;
  a=1; b=SIZE_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SIZE_MAX; EXPECT_FALSE(safe_inc(&a));
  a=SIZE_MAX/2; b=a+2; EXPECT_FALSE(safe_add(NULL, a, b));
  a=SIZE_MAX/2; b=a; EXPECT_TRUE(safe_add(NULL, a, b));
  a=SIZE_MAX/2; b=a+1; EXPECT_TRUE(safe_add(NULL, a, b));
  a=10; b=11; EXPECT_TRUE(safe_add(NULL, a, b));
  a=0; b=SIZE_MAX; EXPECT_TRUE(safe_add(NULL, a, b));
  return r;
}

int T_add_mixed() {
  int r=1;
  int8_t a = 1;
  uint8_t b = 2;
  uint16_t c = 3;
  a=1; b=SCHAR_MAX; EXPECT_FALSE(safe_add(NULL, a, b));
  a=0; b=SCHAR_MAX+1; EXPECT_FALSE(safe_add(NULL, a, b));
  a=1; b=SCHAR_MAX-1; EXPECT_TRUE(safe_add(NULL, a, b));
  b=1; c=UCHAR_MAX; EXPECT_FALSE(safe_add(NULL, b, c));
  b=0; c=UCHAR_MAX+1; EXPECT_FALSE(safe_add(NULL, b, c));
  b=1; c=UCHAR_MAX-1; EXPECT_TRUE(safe_add(NULL, b, c));
  b=1; c=UCHAR_MAX-1; EXPECT_TRUE(safe_add(NULL, c, b));
  a=1; c=USHRT_MAX; EXPECT_FALSE(safe_add(NULL, a, c));
  a=1;b=1;c=USHRT_MAX-3; EXPECT_FALSE(safe_add3(NULL, a, b, c));
  a=1;b=1;c=1; EXPECT_TRUE(safe_add3(NULL, a, b, c));
  a=1;b=1;c=SCHAR_MAX-3; EXPECT_TRUE(safe_add3(NULL, a, b, c));
  return r;
}

int T_add_increment() {
  int r=1;
  uint16_t a = 1, b = 2, c = 0, d[2]= {0};
  uint16_t *cur = d;
  EXPECT_TRUE(safe_add(cur++, a++, b));
  EXPECT_TRUE(cur == &d[1]);
  EXPECT_TRUE(d[0] == 3);
  EXPECT_TRUE(a == 2);
  a = 1; b = 2; c = 1; cur=d;d[0] = 0;
  EXPECT_TRUE(safe_add3(cur++, a++, b++, c));
  EXPECT_TRUE(d[0] == 4);
  EXPECT_TRUE(cur == &d[1]);
  EXPECT_TRUE(a == 2);
  EXPECT_TRUE(b == 3);
  EXPECT_TRUE(c == 1);
  a = 1; b = 2; cur=d;d[0] = 0;
  EXPECT_TRUE(safe_add(cur++, a++, b++));
  EXPECT_TRUE(d[0] == 3);
  EXPECT_TRUE(cur == &d[1]);
  EXPECT_TRUE(a == 2);
  EXPECT_TRUE(b == 3);

  return r;
}



/***** SUB *****/
int T_sub_s8() {
  int r=1;
  int8_t a, b;
  a=SCHAR_MIN; b=1; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SCHAR_MIN; EXPECT_FALSE(safe_dec(&a));
  a=1; EXPECT_TRUE(safe_dec(&a)); EXPECT_TRUE(a==0);
  a=SCHAR_MIN; b=SCHAR_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SCHAR_MIN/2; b=SCHAR_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=-2; b=SCHAR_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SCHAR_MAX; b=SCHAR_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=2; b=10; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_s16() {
  int r=1;
  int16_t a, b;
  a=SHRT_MIN; b=1; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SHRT_MIN; EXPECT_FALSE(safe_dec(&a));
  a=SHRT_MIN; b=SHRT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SHRT_MIN/2; b=SHRT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=-2; b=SHRT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SHRT_MAX; b=SHRT_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=2; b=10; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_s32() {
  int r=1;
  int32_t a, b;
  a=INT_MIN; b=1; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=INT_MIN; EXPECT_FALSE(safe_dec(&a));
  a=INT_MIN; b=INT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=INT_MIN/2; b=INT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=-2; b=INT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=INT_MAX; b=INT_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=2; b=10; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_s64() {
  int r=1;
  int64_t a, b;
  a=SAFE_INT64_MIN; b=1; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SAFE_INT64_MIN; EXPECT_FALSE(safe_dec(&a));
  a=SAFE_INT64_MIN; b=SAFE_INT64_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SAFE_INT64_MIN/2; b=SAFE_INT64_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=-2; b=SAFE_INT64_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SAFE_INT64_MAX; b=SAFE_INT64_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=2; b=10; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_long() {
  int r=1;
  long a, b;
  a=LONG_MIN; b=1; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=LONG_MIN; EXPECT_FALSE(safe_dec(&a));
  a=LONG_MIN; b=LONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=LONG_MIN/2; b=LONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=-2; b=LONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=LONG_MAX; b=LONG_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=2; b=10; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_longlong() {
  int r=1;
  long long a, b;
  a=LLONG_MIN; b=1; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=LLONG_MIN; EXPECT_FALSE(safe_dec(&a));
  a=LLONG_MIN; b=LLONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=LLONG_MIN/2; b=LLONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=-2; b=LLONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=LLONG_MAX; b=LLONG_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=2; b=10; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_ssizet() {
  int r=1;
  ssize_t a, b;
  a=SSIZE_MIN; b=1; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SSIZE_MIN; EXPECT_FALSE(safe_dec(&a));
  a=SSIZE_MIN; b=SSIZE_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SSIZE_MIN/2; b=SSIZE_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=-2; b=SSIZE_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=SSIZE_MAX; b=SSIZE_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=2; b=10; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_u8() {
  int r=1;
  uint8_t a, b;
  a=0; b=UCHAR_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=0; EXPECT_FALSE(safe_dec(&a));
  a=UCHAR_MAX-1; b=UCHAR_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=UCHAR_MAX; b=UCHAR_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=1; b=100; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_u16() {
  int r=1;
  uint16_t a, b;
  a=0; b=USHRT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=0; EXPECT_FALSE(safe_dec(&a));
  a=USHRT_MAX-1; b=USHRT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=USHRT_MAX; b=USHRT_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=1; b=100; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_u32() {
  int r=1;
  uint32_t a, b;
  a=UINT_MAX-1; b=UINT_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=0; EXPECT_FALSE(safe_dec(&a));
  a=UINT_MAX; b=UINT_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=1; b=100; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_u64() {
  int r=1;
  uint64_t a, b;
  a=SAFE_UINT64_MAX-1; b=SAFE_UINT64_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=0; EXPECT_FALSE(safe_dec(&a));
  a=SAFE_UINT64_MAX; b=SAFE_UINT64_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=1; b=100; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_ulong() {
  int r=1;
  unsigned long a, b;
  a=ULONG_MAX-1; b=ULONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=0; EXPECT_FALSE(safe_dec(&a));
  a=ULONG_MAX; b=ULONG_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=1; b=100; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_ulonglong() {
  int r=1;
  unsigned long long a, b;
  a=ULLONG_MAX-1; b=ULLONG_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=0; EXPECT_FALSE(safe_dec(&a));
  a=ULLONG_MAX; b=ULLONG_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=1; b=100; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

int T_sub_sizet() {
  int r=1;
  size_t a, b;
  a=SIZE_MAX-1; b=SIZE_MAX; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=0; EXPECT_FALSE(safe_dec(&a));
  a=SIZE_MAX; b=SIZE_MAX; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=1; b=100; EXPECT_FALSE(safe_sub(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_sub(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_sub(NULL, a, b));
  return r;
}

/***** MUL *****/
int T_mul_s8() {
  int r=1;
  int8_t a, b;
  a=SCHAR_MIN; b=-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SCHAR_MIN; b=-2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SCHAR_MAX; b=SCHAR_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SCHAR_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SCHAR_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SCHAR_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SCHAR_MIN; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SCHAR_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SCHAR_MIN; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_s16() {
  int r=1;
  int16_t a, b;
  a=SHRT_MIN; b=-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SHRT_MIN; b=-2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SHRT_MAX; b=SHRT_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SHRT_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SHRT_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SHRT_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SHRT_MIN; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SHRT_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SHRT_MIN; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_s32() {
  int r=1;
  int32_t a, b;
  a=INT_MIN; b=-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=INT_MIN; b=-2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=INT_MAX; b=INT_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=INT_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=INT_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=INT_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=INT_MIN; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=INT_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=INT_MIN; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_s64() {
  int r=1;
  int64_t a, b;
  a=SAFE_INT64_MIN; b=-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SAFE_INT64_MIN; b=-2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SAFE_INT64_MAX; b=SAFE_INT64_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SAFE_INT64_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SAFE_INT64_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SAFE_INT64_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SAFE_INT64_MIN; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SAFE_INT64_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SAFE_INT64_MIN; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_long() {
  int r=1;
  long a, b;
  a=LONG_MIN; b=-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LONG_MIN; b=-2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LONG_MAX; b=LONG_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LONG_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LONG_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=LONG_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=LONG_MIN; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=LONG_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=LONG_MIN; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}
int T_mul_longlong() {
  int r=1;
  long long a, b;
  a=LLONG_MIN; b=-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LLONG_MIN; b=-2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LLONG_MAX; b=LLONG_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LLONG_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=LLONG_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=LLONG_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=LLONG_MIN; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=LLONG_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=LLONG_MIN; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}
int T_mul_ssizet() {
  int r=1;
  ssize_t a, b;
  a=SSIZE_MIN; b=-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SSIZE_MIN; b=-2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SSIZE_MAX; b=SSIZE_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SSIZE_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SSIZE_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=100; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SSIZE_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SSIZE_MIN; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SSIZE_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SSIZE_MIN; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_u8() {
  int r=1;
  uint8_t a, b;
  a=UCHAR_MAX-1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=UCHAR_MAX-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=UCHAR_MAX; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=UCHAR_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=UCHAR_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=UCHAR_MAX/2+1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=UCHAR_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=UCHAR_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=1; b=UCHAR_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=UCHAR_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=UCHAR_MAX; b=1; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_u16() {
  int r=1;
  uint16_t a, b;
  a=USHRT_MAX-1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=USHRT_MAX-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=USHRT_MAX; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=USHRT_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=USHRT_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=USHRT_MAX/2+1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=USHRT_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=USHRT_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=1; b=USHRT_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=USHRT_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=USHRT_MAX; b=1; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_u32() {
  int r=1;
  uint32_t a, b;
  a=UINT_MAX-1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=UINT_MAX-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=UINT_MAX; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=UINT_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=UINT_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=UINT_MAX/2+1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=UINT_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=UINT_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=1; b=UINT_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=UINT_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=UINT_MAX; b=1; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_u64() {
  int r=1;
  uint64_t a, b;
  a=SAFE_UINT64_MAX-1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=SAFE_UINT64_MAX-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SAFE_UINT64_MAX; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=SAFE_UINT64_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SAFE_UINT64_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=SAFE_UINT64_MAX/2+1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SAFE_UINT64_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SAFE_UINT64_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=1; b=SAFE_UINT64_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SAFE_UINT64_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SAFE_UINT64_MAX; b=1; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_ulong() {
  int r=1;
  unsigned long a, b;
  a=ULONG_MAX-1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=ULONG_MAX-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=ULONG_MAX; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=ULONG_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=ULONG_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=ULONG_MAX/2+1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=ULONG_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=ULONG_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=1; b=ULONG_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=ULONG_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=ULONG_MAX; b=1; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_ulonglong() {
  int r=1;
  unsigned long long a, b;
  a=ULLONG_MAX-1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=ULLONG_MAX-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=ULLONG_MAX; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=ULLONG_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=ULLONG_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=ULLONG_MAX/2+1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=ULLONG_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=ULLONG_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=1; b=ULLONG_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=ULLONG_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=ULLONG_MAX; b=1; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

int T_mul_sizet() {
  int r=1;
  size_t a, b;
  a=SIZE_MAX-1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=SIZE_MAX-1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SIZE_MAX; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=SIZE_MAX; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SIZE_MAX/2+1; b=2; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=2; b=SIZE_MAX/2+1; EXPECT_FALSE(safe_mul(NULL, a, b));
  a=SIZE_MAX/2; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=0; b=SIZE_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=1; b=SIZE_MAX; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SIZE_MAX; b=0; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=SIZE_MAX; b=1; EXPECT_TRUE(safe_mul(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mul(NULL, a, b));
  return r;
}

/***** MOD *****/
int T_mod_s8() {
  int r=1;
  int8_t a, b;
  a=SCHAR_MIN; b=-1; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_s16() {
  int r=1;
  int16_t a, b;
  a=SHRT_MIN; b=-1; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_s32() {
  int r=1;
  int32_t a, b;
  a=INT_MIN; b=-1; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_s64() {
  int r=1;
  int64_t a, b;
  a=SAFE_INT64_MIN; b=-1; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_long() {
  int r=1;
  long a, b;
  a=LONG_MIN; b=-1; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}
int T_mod_longlong() {
  int r=1;
  long long a, b;
  a=LLONG_MIN; b=-1LL; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=100LL; b=0LL; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10LL; b=2LL; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}
int T_mod_ssizet() {
  int r=1;
  ssize_t a, b;
  a=SSIZE_MIN; b=-1; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_u8() {
  int r=1;
  uint8_t a, b;
  a=0; b=UCHAR_MAX; EXPECT_TRUE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_u16() {
  int r=1;
  uint16_t a, b;
  a=0; b=USHRT_MAX; EXPECT_TRUE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_u32() {
  int r=1;
  uint32_t a, b;
  a=0; b=UINT_MAX; EXPECT_TRUE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_u64() {
  int r=1;
  uint64_t a, b;
  a=0; b=SAFE_INT64_MAX; EXPECT_TRUE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_ulong() {
  int r=1;
  unsigned long a, b;
  a=0; b=LONG_MAX; EXPECT_TRUE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_ulonglong() {
  int r=1;
  unsigned long long a, b;
  a=0ULL; b=~0ULL; EXPECT_TRUE(safe_mod(NULL, a, b));
  a=100ULL; b=0ULL; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10ULL; b=2ULL; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

int T_mod_sizet() {
  int r=1;
  size_t a, b;
  a=0; b=SIZE_MAX; EXPECT_TRUE(safe_mod(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_mod(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_mod(NULL, a, b));
  return r;
}

/***** DIV *****/
int T_div_s8() {
  int r=1;
  int8_t a, b;
  a=SCHAR_MIN; b=-1; EXPECT_FALSE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_s16() {
  int r=1;
  int16_t a, b;
  a=SHRT_MIN; b=-1; EXPECT_FALSE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_s32() {
  int r=1;
  int32_t a, b;
  a=INT_MIN; b=-1; EXPECT_FALSE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_s64() {
  int r=1;
  int64_t a, b;
  a=SAFE_INT64_MIN; b=-1; EXPECT_FALSE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_long() {
  int r=1;
  long a, b;
  a=LONG_MIN; b=-1; EXPECT_FALSE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}
int T_div_longlong() {
  int r=1;
  long long a, b;
  a=LLONG_MIN; b=-1LL; EXPECT_FALSE(safe_div(NULL, a, b));
  a=100LL; b=0LL; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10LL; b=2LL; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}
int T_div_ssizet() {
  int r=1;
  ssize_t a, b;
  a=SSIZE_MIN; b=-1; EXPECT_FALSE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_u8() {
  int r=1;
  uint8_t a, b;
  a=0; b=UCHAR_MAX; EXPECT_TRUE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_u16() {
  int r=1;
  uint16_t a, b;
  a=0; b=USHRT_MAX; EXPECT_TRUE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_u32() {
  int r=1;
  uint32_t a, b;
  a=0; b=UINT_MAX; EXPECT_TRUE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_u64() {
  int r=1;
  uint64_t a, b;
  a=0; b=SAFE_INT64_MAX; EXPECT_TRUE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_ulong() {
  int r=1;
  unsigned long a, b;
  a=0; b=LONG_MAX; EXPECT_TRUE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_ulonglong() {
  int r=1;
  unsigned long long a, b;
  a=0ULL; b=~0ULL; EXPECT_TRUE(safe_div(NULL, a, b));
  a=100ULL; b=0ULL; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10ULL; b=2ULL; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

int T_div_sizet() {
  int r=1;
  size_t a, b;
  a=0; b=SIZE_MAX; EXPECT_TRUE(safe_div(NULL, a, b));
  a=100; b=0; EXPECT_FALSE(safe_div(NULL, a, b));
  a=10; b=2; EXPECT_TRUE(safe_div(NULL, a, b));
  return r;
}

/***** SHL *****/
int T_shl_s8() {
  int r=1;
  int8_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int8_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int8_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=5; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_s16() {
  int r=1;
  int16_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int16_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int16_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_s32() {
  int r=1;
  int32_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int32_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int32_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_s64() {
  int r=1;
  int64_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int64_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(int64_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_long() {
  int r=1;
  long a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}
int T_shl_longlong() {
  int r=1;
  long long a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(long long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(long long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}
int T_shl_ssizet() {
  int r=1;
  ssize_t a, b;
   a=-1; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(ssize_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=sizeof(ssize_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
 return r;
}

int T_shl_u8() {
  int r=1;
  uint8_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=UCHAR_MAX; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_u16() {
  int r=1;
  uint16_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=USHRT_MAX; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_u32() {
  int r=1;
  uint32_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=UINT_MAX; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_u64() {
  int r=1;
  uint64_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=SAFE_UINT64_MAX; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_ulong() {
  int r=1;
  unsigned long a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=ULONG_MAX; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_ulonglong() {
  int r=1;
  unsigned long long a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=ULLONG_MAX; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

int T_shl_sizet() {
  int r=1;
  size_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shl(NULL,a, b));
  a=SIZE_MAX; b=1; EXPECT_FALSE(safe_shl(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shl(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shl(NULL, a, b));
  return r;
}

/***** SHR *****/
int T_shr_s8() {
  int r=1;
  int8_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int8_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int8_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=5; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_s16() {
  int r=1;
  int16_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int16_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int16_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_s32() {
  int r=1;
  int32_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int32_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int32_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_s64() {
  int r=1;
  int64_t a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int64_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(int64_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_long() {
  int r=1;
  long a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}
int T_shr_longlong() {
  int r=1;
  long long a, b;
  a=-1; b=1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(long long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(long long)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}
int T_shr_ssizet() {
  int r=1;
  ssize_t a, b;
   a=-1; b=1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=-1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(ssize_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=sizeof(ssize_t)*CHAR_BIT + 1; EXPECT_FALSE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=100; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
 return r;
}

int T_shr_u8() {
  int r=1;
  uint8_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_u16() {
  int r=1;
  uint16_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_u32() {
  int r=1;
  uint32_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_u64() {
  int r=1;
  uint64_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_ulong() {
  int r=1;
  unsigned long a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_ulonglong() {
  int r=1;
  unsigned long long a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

int T_shr_sizet() {
  int r=1;
  size_t a, b;
  a=1; b=sizeof(typeof(a))*CHAR_BIT+1; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=4; b=sizeof(typeof(a))*CHAR_BIT; EXPECT_FALSE(safe_shr(NULL,a, b));
  a=1; b=2; EXPECT_TRUE(safe_shr(NULL, a, b));
  a=1; b=4; EXPECT_TRUE(safe_shr(NULL, a, b));
  return r;
}

/***** MISC *****/

int T_magic_constants() {
  int r=1;
  EXPECT_TRUE(__sio(m)(smin)(((int8_t)0)) == SCHAR_MIN);
  EXPECT_TRUE(__sio(m)(smax)(((int8_t)0)) == SCHAR_MAX);
  EXPECT_TRUE(__sio(m)(umax)(((uint8_t)0)) == UCHAR_MAX);

  EXPECT_TRUE(__sio(m)(smin)(((int16_t)0)) == SHRT_MIN);
  EXPECT_TRUE(__sio(m)(smax)(((int16_t)0)) == SHRT_MAX);
  EXPECT_TRUE(__sio(m)(umax)(((uint16_t)0)) == USHRT_MAX);

  EXPECT_TRUE(__sio(m)(smin)(((int32_t)0)) == INT_MIN);
  EXPECT_TRUE(__sio(m)(smax)(((int32_t)0)) == INT_MAX);
  EXPECT_TRUE(__sio(m)(umax)(((uint32_t)0)) == UINT_MAX);

  EXPECT_TRUE(__sio(m)(smin)(((int64_t)0)) == SAFE_INT64_MIN);
  EXPECT_TRUE(__sio(m)(smax)(((int64_t)0)) == SAFE_INT64_MAX);
  EXPECT_TRUE(__sio(m)(umax)(((uint64_t)0)) == SAFE_UINT64_MAX);

  EXPECT_TRUE(__sio(m)(smin)(((ssize_t)0)) == SSIZE_MIN);
  EXPECT_TRUE(__sio(m)(smax)(((ssize_t)0)) == SSIZE_MAX);
  EXPECT_TRUE(__sio(m)(umax)(((size_t)0)) == SIZE_MAX);

  EXPECT_TRUE(__sio(m)(smin)(((long)0)) == LONG_MIN);
  EXPECT_TRUE(__sio(m)(smax)(((long)0)) == LONG_MAX);
  EXPECT_TRUE(__sio(m)(umax)(((unsigned long)0)) == ULONG_MAX);

  EXPECT_TRUE(__sio(m)(smin)(((long long)0)) == LLONG_MIN);
  EXPECT_TRUE(__sio(m)(smax)(((long long)0)) == LLONG_MAX);
  EXPECT_TRUE(__sio(m)(umax)(((unsigned long long)0)) == ULLONG_MAX);

  return r;
}

#ifdef SAFE_IOP_SPEED_TEST
#include <sys/time.h>
#include <time.h>

#define SPEED_TEST(_type, _tests, _ops, _op, _fn) ({ \
  int tnum; \
  printf("%s: speed test(" #_type ", %d, %u, %s)\n", \
         __func__, _tests, _ops, #_op); \
  for (tnum=0; tnum < (_tests); ++tnum) { \
    unsigned int speed_i = 0; \
    _type speed_a=0x41, speed_b=0x42, speed_c; \
    struct timeval start, finish; \
    double raw=0, safe=0; \
    gettimeofday(&start, NULL); \
    for (speed_c=0,speed_i=0; speed_i < _ops; ++speed_i) \
      speed_c = speed_a _op speed_b; \
    for (speed_c=0,speed_i=0; speed_i < _ops; ++speed_i) \
      speed_c = speed_a _op speed_b; \
    for (speed_c=0,speed_i=0; speed_i < _ops; ++speed_i) \
      speed_c = speed_a _op speed_b; \
    gettimeofday(&finish, NULL); \
    raw = finish.tv_sec - start.tv_sec + \
          (finish.tv_usec - start.tv_usec) / 1.e6; \
    gettimeofday(&start, NULL); \
    for (speed_c=0,speed_i=0; speed_i < _ops; ++speed_i) \
      _fn(&speed_c, speed_a, speed_b); \
    for (speed_c=0,speed_i=0; speed_i < _ops; ++speed_i) \
      _fn(&speed_c, speed_a, speed_b); \
    for (speed_c=0,speed_i=0; speed_i < _ops; ++speed_i) \
      _fn(&speed_c, speed_a, speed_b); \
    gettimeofday(&finish, NULL); \
    safe = finish.tv_sec - start.tv_sec + \
          (finish.tv_usec - start.tv_usec) / 1.e6; \
    printf("%s: [%d] %u*3 ops; raw: %.9fs safe: %.9fs\n", \
           __func__, tnum, speed_i, raw, safe); \
  } \
})

int T_speed() {
  int r=1, truns=2;
  unsigned int runs = UINT_MAX;
  SPEED_TEST(size_t, truns, runs, +, safe_add);
  SPEED_TEST(unsigned long long, truns, runs, +, safe_add);
  SPEED_TEST(unsigned long, truns, runs, +, safe_add);
  SPEED_TEST(uint64_t, truns, runs, +, safe_add);
  SPEED_TEST(uint32_t, truns, runs, +, safe_add);
  SPEED_TEST(uint16_t, truns, runs, +, safe_add);
  SPEED_TEST(uint8_t, truns, runs, +, safe_add);
  SPEED_TEST(ssize_t, truns, runs, +, safe_add);
  SPEED_TEST(long long, truns, runs, +, safe_add);
  SPEED_TEST(long, truns, runs, +, safe_add);
  SPEED_TEST(int64_t, truns, runs, +, safe_add);
  SPEED_TEST(int32_t, truns, runs, +, safe_add);
  SPEED_TEST(int16_t, truns, runs, +, safe_add);
  SPEED_TEST(int8_t, truns, runs, +, safe_add);

  SPEED_TEST(size_t, truns, runs, -, safe_sub);
  SPEED_TEST(unsigned long long, truns, runs, -, safe_sub);
  SPEED_TEST(unsigned long, truns, runs, -, safe_sub);
  SPEED_TEST(uint64_t, truns, runs, -, safe_sub);
  SPEED_TEST(uint32_t, truns, runs, -, safe_sub);
  SPEED_TEST(uint16_t, truns, runs, -, safe_sub);
  SPEED_TEST(uint8_t, truns, runs, -, safe_sub);
  SPEED_TEST(ssize_t, truns, runs, -, safe_sub);
  SPEED_TEST(long long, truns, runs, -, safe_sub);
  SPEED_TEST(long, truns, runs, -, safe_sub);
  SPEED_TEST(int64_t, truns, runs, -, safe_sub);
  SPEED_TEST(int32_t, truns, runs, -, safe_sub);
  SPEED_TEST(int16_t, truns, runs, -, safe_sub);
  SPEED_TEST(int8_t, truns, runs, -, safe_sub);

  SPEED_TEST(size_t, truns, runs, *, safe_mul);
  SPEED_TEST(unsigned long long, truns, runs, *, safe_mul);
  SPEED_TEST(unsigned long, truns, runs, *, safe_mul);
  SPEED_TEST(uint64_t, truns, runs, *, safe_mul);
  SPEED_TEST(uint32_t, truns, runs, *, safe_mul);
  SPEED_TEST(uint16_t, truns, runs, *, safe_mul);
  SPEED_TEST(uint8_t, truns, runs, *, safe_mul);
  SPEED_TEST(ssize_t, truns, runs, *, safe_mul);
  SPEED_TEST(long long, truns, runs, *, safe_mul);
  SPEED_TEST(long, truns, runs, *, safe_mul);
  SPEED_TEST(int64_t, truns, runs, *, safe_mul);
  SPEED_TEST(int32_t, truns, runs, *, safe_mul);
  SPEED_TEST(int16_t, truns, runs, *, safe_mul);
  SPEED_TEST(int8_t, truns, runs, *, safe_mul);

  SPEED_TEST(size_t, truns, runs, /, safe_div);
  SPEED_TEST(unsigned long long, truns, runs, /, safe_div);
  SPEED_TEST(unsigned long, truns, runs, /, safe_div);
  SPEED_TEST(uint64_t, truns, runs, /, safe_div);
  SPEED_TEST(uint32_t, truns, runs, /, safe_div);
  SPEED_TEST(uint16_t, truns, runs, /, safe_div);
  SPEED_TEST(uint8_t, truns, runs, /, safe_div);
  SPEED_TEST(ssize_t, truns, runs, /, safe_div);
  SPEED_TEST(long long, truns, runs, /, safe_div);
  SPEED_TEST(long, truns, runs, /, safe_div);
  SPEED_TEST(int64_t, truns, runs, /, safe_div);
  SPEED_TEST(int32_t, truns, runs, /, safe_div);
  SPEED_TEST(int16_t, truns, runs, /, safe_div);
  SPEED_TEST(int8_t, truns, runs, /, safe_div);

  SPEED_TEST(size_t, truns, runs, %, safe_mod);
  SPEED_TEST(unsigned long long, truns, runs, %, safe_mod);
  SPEED_TEST(unsigned long, truns, runs, %, safe_mod);
  SPEED_TEST(uint64_t, truns, runs, %, safe_mod);
  SPEED_TEST(uint32_t, truns, runs, %, safe_mod);
  SPEED_TEST(uint16_t, truns, runs, %, safe_mod);
  SPEED_TEST(uint8_t, truns, runs, %, safe_mod);
  SPEED_TEST(ssize_t, truns, runs, %, safe_mod);
  SPEED_TEST(long long, truns, runs, %, safe_mod);
  SPEED_TEST(long, truns, runs, %, safe_mod);
  SPEED_TEST(int64_t, truns, runs, %, safe_mod);
  SPEED_TEST(int32_t, truns, runs, %, safe_mod);
  SPEED_TEST(int16_t, truns, runs, %, safe_mod);
  SPEED_TEST(int8_t, truns, runs, %, safe_mod);

  return r;
}
#endif

int main(int argc, char **argv) {
  /* test inlines */
  int tests = 0, succ = 0, fail = 0;
  tests++; if (T_shr_s8())  succ++; else fail++;
  tests++; if (T_shr_s16()) succ++; else fail++;
  tests++; if (T_shr_s32()) succ++; else fail++;
  tests++; if (T_shr_s64()) succ++; else fail++;
  tests++; if (T_shr_long()) succ++; else fail++;
  tests++; if (T_shr_longlong()) succ++; else fail++;
  tests++; if (T_shr_ssizet()) succ++; else fail++;
  tests++; if (T_shr_u8())  succ++; else fail++;
  tests++; if (T_shr_u16()) succ++; else fail++;
  tests++; if (T_shr_u32()) succ++; else fail++;
  tests++; if (T_shr_u64()) succ++; else fail++;
  tests++; if (T_shr_ulong()) succ++; else fail++;
  tests++; if (T_shr_ulonglong()) succ++; else fail++;
  tests++; if (T_shr_sizet()) succ++; else fail++;

  tests++; if (T_shl_s8())  succ++; else fail++;
  tests++; if (T_shl_s16()) succ++; else fail++;
  tests++; if (T_shl_s32()) succ++; else fail++;
  tests++; if (T_shl_s64()) succ++; else fail++;
  tests++; if (T_shl_long()) succ++; else fail++;
  tests++; if (T_shl_longlong()) succ++; else fail++;
  tests++; if (T_shl_ssizet()) succ++; else fail++;
  tests++; if (T_shl_u8())  succ++; else fail++;
  tests++; if (T_shl_u16()) succ++; else fail++;
  tests++; if (T_shl_u32()) succ++; else fail++;
  tests++; if (T_shl_u64()) succ++; else fail++;
  tests++; if (T_shl_ulong()) succ++; else fail++;
  tests++; if (T_shl_ulonglong()) succ++; else fail++;
  tests++; if (T_shl_sizet()) succ++; else fail++;

  tests++; if (T_div_s8())  succ++; else fail++;
  tests++; if (T_div_s16()) succ++; else fail++;
  tests++; if (T_div_s32()) succ++; else fail++;
  tests++; if (T_div_s64()) succ++; else fail++;
  tests++; if (T_div_long()) succ++; else fail++;
  tests++; if (T_div_longlong()) succ++; else fail++;
  tests++; if (T_div_ssizet()) succ++; else fail++;
  tests++; if (T_div_u8())  succ++; else fail++;
  tests++; if (T_div_u16()) succ++; else fail++;
  tests++; if (T_div_u32()) succ++; else fail++;
  tests++; if (T_div_u64()) succ++; else fail++;
  tests++; if (T_div_ulong()) succ++; else fail++;
  tests++; if (T_div_ulonglong()) succ++; else fail++;
  tests++; if (T_div_sizet()) succ++; else fail++;

  tests++; if (T_mod_s8())  succ++; else fail++;
  tests++; if (T_mod_s16()) succ++; else fail++;
  tests++; if (T_mod_s32()) succ++; else fail++;
  tests++; if (T_mod_s64()) succ++; else fail++;
  tests++; if (T_mod_long()) succ++; else fail++;
  tests++; if (T_mod_longlong()) succ++; else fail++;
  tests++; if (T_mod_ssizet()) succ++; else fail++;
  tests++; if (T_mod_u8())  succ++; else fail++;
  tests++; if (T_mod_u16()) succ++; else fail++;
  tests++; if (T_mod_u32()) succ++; else fail++;
  tests++; if (T_mod_u64()) succ++; else fail++;
  tests++; if (T_mod_ulong()) succ++; else fail++;
  tests++; if (T_mod_ulonglong()) succ++; else fail++;
  tests++; if (T_mod_sizet()) succ++; else fail++;

  tests++; if (T_mul_s8())  succ++; else fail++;
  tests++; if (T_mul_s16()) succ++; else fail++;
  tests++; if (T_mul_s32()) succ++; else fail++;
  tests++; if (T_mul_s64()) succ++; else fail++;
  tests++; if (T_mul_long()) succ++; else fail++;
  tests++; if (T_mul_longlong()) succ++; else fail++;
  tests++; if (T_mul_ssizet()) succ++; else fail++;
  tests++; if (T_mul_u8())  succ++; else fail++;
  tests++; if (T_mul_u16()) succ++; else fail++;
  tests++; if (T_mul_u32()) succ++; else fail++;
  tests++; if (T_mul_u64()) succ++; else fail++;
  tests++; if (T_mul_ulong()) succ++; else fail++;
  tests++; if (T_mul_ulonglong()) succ++; else fail++;
  tests++; if (T_mul_sizet()) succ++; else fail++;

  tests++; if (T_sub_s8())  succ++; else fail++;
  tests++; if (T_sub_s16()) succ++; else fail++;
  tests++; if (T_sub_s32()) succ++; else fail++;
  tests++; if (T_sub_s64()) succ++; else fail++;
  tests++; if (T_sub_long()) succ++; else fail++;
  tests++; if (T_sub_longlong()) succ++; else fail++;
  tests++; if (T_sub_ssizet()) succ++; else fail++;
  tests++; if (T_sub_u8())  succ++; else fail++;
  tests++; if (T_sub_u16()) succ++; else fail++;
  tests++; if (T_sub_u32()) succ++; else fail++;
  tests++; if (T_sub_u64()) succ++; else fail++;
  tests++; if (T_sub_ulong()) succ++; else fail++;
  tests++; if (T_sub_ulonglong()) succ++; else fail++;
  tests++; if (T_sub_sizet()) succ++; else fail++;

  tests++; if (T_add_s8())  succ++; else fail++;
  tests++; if (T_add_s16()) succ++; else fail++;
  tests++; if (T_add_s32()) succ++; else fail++;
  tests++; if (T_add_s64()) succ++; else fail++;
  tests++; if (T_add_long()) succ++; else fail++;
  tests++; if (T_add_longlong()) succ++; else fail++;
  tests++; if (T_add_ssizet()) succ++; else fail++;
  tests++; if (T_add_u8())  succ++; else fail++;
  tests++; if (T_add_u16()) succ++; else fail++;
  tests++; if (T_add_u32()) succ++; else fail++;
  tests++; if (T_add_u64()) succ++; else fail++;
  tests++; if (T_add_ulong()) succ++; else fail++;
  tests++; if (T_add_ulonglong()) succ++; else fail++;
  tests++; if (T_add_sizet()) succ++; else fail++;
  tests++; if (T_add_mixed()) succ++; else fail++;
  tests++; if (T_add_increment()) succ++; else fail++;

  tests++; if (T_magic_constants()) succ++; else fail++;


  printf("%d/%d expects succeeded (%d failures)\n",
         expect_succ, expect, expect_fail);
  printf("%d/%d tests succeeded (%d failures)\n", succ, tests, fail);
  /* Currently, this requires a quiescent system to be even approximately useful.
   * TODO: use better timing functions */
#ifdef SAFE_IOP_SPEED_TEST
  T_speed();
#endif
  return fail;
}
#endif
