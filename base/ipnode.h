/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_IPNODE_H_
#define BASE_IPNODE_H_

#include <stddef.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"{
#endif

struct ip_record {
    size_t sp;
    size_t max_depth;
    void **ip;
};

struct ip_array {
    void **ip;
    size_t n;
};

static inline int ip_copy(struct ip_record *node, void *const ip[], size_t n) {
    if (node == NULL || ip == NULL)
        return -EINVAL;
    size_t size = (node->sp < n)? node->sp: n;
    size_t i = 0, sp = node->sp;
    while (size > 0) {
        node->ip[sp - i - 1] = (void *)ip[i];
        i++;
        size--;
    }
    node->sp = sp - i;
    return 0;
}

static inline size_t ip_size(const struct ip_record *n) {
    return n->max_depth - n->sp;
}

static inline void **ip_first(const struct ip_record *n) {
    return (void **)(n->ip + n->sp);
}

#ifdef __cplusplus
}
#endif
#endif // BASE_IPNODE_H_
