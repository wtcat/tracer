/*
 * Copyright 2022 wtcat
 */
#ifndef CONTAINER_UTILS_H_
#define CONTAINER_UTILS_H_

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
#endif /* CONTAINER_UTILS_H_ */
