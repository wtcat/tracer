/*
 * Copyright 2022 wtcat
 */
#ifndef TRACER_CORE_H_
#define TRACER_CORE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PATH_RESERVED_SIZE 3
#define PATH_DEFINE(_name, _depth) \
    void *_name[PATH_RESERVED_SIZE + _depth]

typedef void* tracer_pnode_t;

int tracer_node_init(tracer_pnode_t d, size_t depth);
int tracer_generate_path(void* tracer, tracer_pnode_t d);
long tracer_transform_path(void* tracer, tracer_pnode_t d, char* buffer, size_t maxlen);
void* tracer_create(const char* separator, int min_limit, int max_limit);
void tracer_destory(void* tracer);

#ifdef __cplusplus
}
#endif
#endif // TRACER_CORE_H_
