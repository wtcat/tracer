/*
 * Copyright 2022 wtcat
 */
#ifndef MEM_TRACER_H_
#define MEM_TRACER_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif
struct printer;
enum mdump_type {
    MEM_SORTED_DUMP,
    MEM_SEQUEUE_DUMP
};

void *mem_tracer_alloc(size_t size);
void mem_tracer_free(void *ptr);
void mem_tracer_dump(const struct printer *vio, enum mdump_type type);
void mem_tracer_destory(void);

#ifdef __cplusplus
}
#endif
#endif // MEM_TRACER_H_
