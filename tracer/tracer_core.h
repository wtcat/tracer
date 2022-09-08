/*
 * Copyright 2022 wtcat
 */
#ifndef TRACER_CORE_H_
#define TRACER_CORE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "base/rb.h"
#include "base/list.h"
#include "base/allocator.h"

#ifdef __cplusplus
extern "C"{
#endif

struct record_tree {
    RBTree_Control root;
    RBTree_Compare compare;    
};

struct record_node {
    int count;
    struct list_head link;
    RBTree_Node node;
    uintptr_t ipkey;
    size_t max_depth;
    size_t sp;
    void **ip;
};

struct record_class {
    struct record_tree tree;
    struct list_head head;
    struct mem_allocator *allocator;
    size_t node_size;
    void *pnode; /* for record_node */
    void *user;
};

static inline size_t core_record_ip_size(const struct record_node *n) {
    return n->max_depth - n->sp;
}

static inline void *core_record_ip(const struct record_node *n) {
    return n->ip + n->sp;
}
int core_record_ip_compare(struct record_node *ln, struct record_node *rn);
struct record_node *core_record_node_allocate(struct record_class *rc, 
    size_t max_depth);
int core_record_copy_ip(struct record_node *node, const void *ip[], size_t n);
int core_record_add(struct record_class *rc, struct record_node *node);
int core_record_del(struct record_class *rc, struct record_node *node);
void core_record_destroy(struct record_class *rc);
void core_record_visitor(struct record_class *rc,
    bool (*iterator)(struct record_node *n, void *u), 
    void *user);

#ifdef __cplusplus
}
#endif
#endif // TRACER_CORE_H_
