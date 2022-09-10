/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/list.h"
#include "base/utils.h"
#include "base/assert.h"
#include "base/printer.h"
#include "tracer/tracer_core.h"

static uint32_t ipkey_generate(uint32_t crc, const uint8_t* data, size_t len) {
    static const uint32_t table[16] = {
        0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
        0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
        0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
        0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU,
    };
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        crc = (crc >> 4) ^ table[(crc ^ byte) & 0x0f];
        crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)byte >> 4)) & 0x0f];
    }
    return ~crc;
}

static int core_record_copy_ip(struct record_node *node, const void *ip[], size_t n) {
    if (node == NULL || ip == NULL)
        return -EINVAL;
    size_t size = MIN(node->sp, n);
    size_t i = 0, sp = node->sp;
    while (size > 0) {
        node->ip[sp - i - 1] = (void *)ip[i];
        i++;
        size--;
    }
    node->sp = sp - i;
    return 0;
}

static void _backtrace_begin(void *user) {
    struct record_class *path = (struct record_class *)user;
    struct record_node *node = (struct record_node *)path->pnode;
    backtrace_begin(&path->tracer, node->context, false);
}

static void _backtrace_entry(struct backtrace_entry *entry, void *user) {
    struct record_class *path = (struct record_class *)user;
    struct record_node *node = (struct record_node *)path->pnode;
    core_record_copy_ip(node, (const void **)entry->ip, entry->n);
}

static void _backtrace_end(void *user) {
    struct record_class *path = (struct record_class *)user;
    struct record_node *node = (struct record_node *)path->pnode; 
    backtrace_end(&path->tracer, node->context, false);
}

static struct backtrace_callbacks callbacks = {
    .begin = _backtrace_begin,
    .callback = _backtrace_entry,
    .end = _backtrace_end
};

rbtree_compare_result core_record_ip_compare(struct record_node *ln, 
    struct record_node *rn) {
    return (long)((intptr_t)ln->ipkey - (intptr_t)rn->ipkey);
}

void core_record_backtrace(struct record_class *rc, struct record_node *node) {
    rc->pnode = node;
    do_backtrace(&rc->tracer, &callbacks, rc);
}

void core_record_print_path(struct record_class *path, struct record_node *node, 
    const struct printer *vio, const char *separator) {
    size_t n = core_record_ip_size(node);
    void **ip = core_record_ip(node);
    char str[256];
    backtrace_begin(&path->tracer, node->context, true);
    for (size_t i = 0; i < n; i++) {
        int ret = backtrace_addr2symbol(&path->tracer, ip[i], str, 
            sizeof(str), node->context);
        if (ret) {
            virt_print(vio, "%s0x%p", separator, ip[i]);
            continue;
        }
        virt_print(vio, "%s%s", separator, str);
    }
    backtrace_end(&path->tracer, node->context, true);
}

struct record_node *core_record_node_allocate(struct record_class *rc, 
    size_t max_depth) {
    ASSERT_TRUE(rc->node_size >= sizeof(struct record_node));
    size_t node_size = (rc->node_size + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1);
    size_t trace_size = backtrace_context_size(&rc->tracer);
    struct record_node *rn = memory_allocate(rc->allocator, 
        node_size + trace_size + max_depth * sizeof(void *), NULL);
    if (rn != NULL) {
        memset(rn, 0, rc->node_size);
        rbtree_set_off_tree(&rn->node);
        rn->max_depth = max_depth;
        rn->ip = (void **)((char *)rn + node_size);
        rn->sp = max_depth;
        rn->context = rn->ip;
    }
    return rn;
}

int core_record_add(struct record_class *rc, struct record_node *node) {
    RBTree_Node *found;
    if (rc == NULL || node == NULL)
        return -EINVAL;
    found = rbtree_insert(&rc->tree.root, &node->node, rc->tree.compare, true);
    ASSERT_TRUE(found == NULL);
    if (!found) {
        node->ipkey = ipkey_generate(0, (void*)&node->ip[node->sp], 
            core_record_ip_size(node));
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
    memory_free(rc->allocator, node, NULL);
    return 0;
}

void core_record_destroy(struct record_class *rc) {
    struct list_head *pos, *next;
    list_for_each_safe(pos, next, &rc->head) {
        struct record_node *rn = CONTAINER_OF(pos, struct record_node, link);
        list_del(pos);
        memory_free(rc->allocator, rn, NULL);
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
