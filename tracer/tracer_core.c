/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/list.h"
#include "base/rb.h"
#include "base/utils.h"
#include "tracer/tracer_core.h"

int core_record_ip_compare(struct record_node *ln, struct record_node *rn) {
    size_t n = ln->max_depth - ln->sp;
    return memcmp(&ln->ip[ln->sp], &rn->ip[rn->sp], n);
}

struct record_node *core_record_node_allocate(struct record_class *rc, 
    size_t max_depth) {
    assert(rc->node_size >= sizeof(struct record_node));
    struct record_node *rn = memory_allocate(rc->allocator, 
        rc->node_size + max_depth * sizeof(uintptr_t));
    if (rn != NULL) {
        memset(rn, 0, rc->node_size);
        rn->node.color = RBT_RED;
        rn->max_depth = max_depth;
        rn->sp = max_depth;
    }
    return rn;
}

int core_record_copy_ip(struct record_node *node, const uintptr_t ip[], size_t n) {
    if (node == NULL || ip == 0)
        return -EINVAL;
    size_t size = MIN(node->sp, n);
    size_t i = 0, sp = node->sp;
    while (size > 0) {
        node->ip[sp - i - 1] = ip[i];
        i++;
        size--;
    }
    node->sp = sp - i;
    return 0;
}

int core_record_add(struct record_class *rc, struct record_node *node) {
    RBTree_Node *found;
    if (rc == NULL || node == NULL)
        return -EINVAL;
    found = rbtree_insert(&rc->tree.root, &node->node, rc->tree.compare, true);
    assert(found == NULL);
    if (!found) {
        list_add_tail(&node->link, &rc->head);
        return 0;
    }
    return -EEXIST;
}

int core_record_del(struct record_class *rc, struct record_node *node) {
    if (rc == NULL || node == NULL)
        return -EINVAL;
    rbtree_extract(&rc->tree.root, &node->node);
    list_del(&node->link);
    memory_free(rc->allocator, node);
    return 0;
}

void core_record_destroy(struct record_class *rc) {
    struct list_head *pos, *next;
    list_for_each_safe(pos, next, &rc->head) {
        struct record_node *rn = CONTAINER_OF(pos, struct record_node, link);
        list_del(pos);
        memory_free(rc->allocator, rn);
    }
}

void core_record_visitor(struct record_class *rc,
    bool (*iterator)(struct record_node *n, void *u), 
    void *user) {
    struct list_head *pos, *next;
    list_for_each_safe(pos, next, &rc->head) {
        struct record_node *n = CONTAINER_OF(pos, struct record_node, link);
        if (!iterator(n, user))
            break;
    }
}
