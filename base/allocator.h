/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_ALLOCATOR_H_
#define BASE_ALLOCATOR_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

struct mem_allocator {
    void *(*allocate)(struct mem_allocator *m, size_t size);
    void (*free)(struct mem_allocator *m, void *ptr);
    void *context;
};

static inline void *memory_allocate(struct mem_allocator *m, size_t size) {
    return m->allocate(m, size);
}

static inline void memory_free(struct mem_allocator *m, void *ptr) {
    m->free(m, ptr);
}

#ifdef __cplusplus
}
#endif
#endif /* BASE_ALLOCATOR_H_ */
