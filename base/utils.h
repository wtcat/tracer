/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_UTILS_H_
#define BASE_UTILS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)ptr - offsetof(type, member)))
#endif

#ifdef __cplusplus
}
#endif
#endif /* BASE_UTILS_H_ */
