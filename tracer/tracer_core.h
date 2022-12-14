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
#include "base/backtrace.h"

#ifdef __cplusplus
extern "C"{
#endif

struct printer;

struct record_tree {
    rbtree_control root;
    rbtree_compare compare;    
};

struct record_node {
    struct list_head link;
    rbtree_node node;
    uintptr_t ipkey;
    struct ip_record ipr;
    /* For tracer */
    void *context;
};

struct record_class {
    struct record_tree tree;
    struct list_head head;
    struct backtrace_class tracer;
    struct mem_allocator *allocator;
    size_t node_size;
    void *pnode; /* for record_node */
    void *user;
};

static inline struct record_node *core_record_lessthen_node(struct record_node *n) {
    rbtree_node *node = rbtree_left(&n->node);
    return CONTAINER_OF(node, struct record_node, node);
}

rbtree_compare_result core_record_ip_compare(struct record_node *ln, 
    struct record_node *rn);
struct record_node *core_record_node_allocate(struct record_class *rc, 
    size_t max_depth);
void core_record_print_path(struct record_class *rc, struct record_node *node, 
    const struct printer *vio, const char *separator);
int core_record_backtrace(struct record_class *rc, struct record_node *node);
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
