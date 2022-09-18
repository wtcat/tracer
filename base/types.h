/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_TYPES_H_
#define BASE_TYPES_H_

#if defined(_WIN32)
#ifndef __type_ssize_t__
#define __type_ssize_t__
typedef long int ssize_t;
#endif

#else
#include <sys/types.h>

#endif
#endif //BASE_TYPES_H_