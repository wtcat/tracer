/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_UTILS_H_
#define BASE_UTILS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

#if defined(__GNUC__) || defined(__clang__)
#define __unused __attribute__((unsed))
#define __pure  __attribute__((pure))
#define likely(_exp)   __builtin_expect(( _exp ), 1)
#define unlikely(_exp) __builtin_expect(( _exp ), 0)
#else

#define __unused
#define __pure
#define likely(_exp)   (_exp)
#define unlikely(_exp) (_exp)
#endif //defined(__GNUC__) || defined(__clang__)

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)ptr - offsetof(type, member)))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b))? (a): (b)
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b))? (a): (b)
#endif

#ifdef __cplusplus
}
#endif
#endif /* BASE_UTILS_H_ */
