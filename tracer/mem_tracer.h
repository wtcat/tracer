/*
 * Copyright 2022 wtcat
 */
#ifndef MEM_TRACER_H_
#define MEM_TRACER_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

#define MTRACER_INST_SIZE 256

struct printer;
enum mdump_type {
    MEM_SORTED_DUMP,
    MEM_SEQUEUE_DUMP
};

#define MTRACER_DEFINE(name) \
    unsigned long name[(MTRACER_INST_SIZE + sizeof(long) - 1) / sizeof(long)]

void *mem_tracer_alloc(void *context, size_t size);
void mem_tracer_free(void *context, void *ptr);
void mem_tracer_dump(void *context, const struct printer *vio, enum mdump_type type);
void mem_tracer_destory(void *context);
void mem_tracer_set_path_length(void *context, size_t maxlen);
void mem_tracer_init(void *context);
void mem_tracer_deinit(void* context);

#ifdef __cplusplus
}
#endif
#endif // MEM_TRACER_H_
