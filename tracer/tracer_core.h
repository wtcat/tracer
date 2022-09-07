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
    unsigned long chksum;
    struct list_head link;
    RBTree_Node node;
    char *sym;
    char *path;
    char *path_ss_;
};

struct record_class {
    struct record_tree tree;
    struct list_head head;
    struct mem_allocator *allocator;
    size_t node_size;
    void *pnode; /* for record_node */
    void *user;
};

int mem_path_append(struct record_node *node, const char *str);
struct record_node *record_node_allocate(struct record_class *rc, 
    size_t symsize, size_t btsize);
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
