/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_ASSERT_H_
#define BASE_ASSERT_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NDEBUG
#define ASSERT_TRUE(_expr) ((void)0)

#else //!NDEBUG

void __assert_func(
    const char* file,
    int         line,
    const char* func,
    const char* failedexpr
);

#define ASSERT_TRUE(_e) \
       (( _e ) ? ( void ) 0 : \
           __assert_func( __FILE__, __LINE__, __func__, #_e ))
#endif //NDEBUG

#ifdef __cplusplus
}
#endif
#endif //BASE_ASSERT_H_