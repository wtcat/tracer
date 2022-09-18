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

static void mem_tracer_begin(struct backtrace_class *cls, void *user) {
    (void) cls;
    (void) user;
}

static void mem_tracer_end(struct backtrace_class *cls, void *user, int err) {
    struct record_node *d = (struct record_node *)user;
    if (!err) {
        struct record_class *rc = CONTAINER_OF(cls, struct record_class, tracer);
        core_record_add(rc, d);
    }
}

static void mem_tracer_entry(const struct backtrace_entry *entry, void *user) {
    struct record_node *d = (struct record_node *)user;
    ip_copy(&d->ipr, entry->ip, entry->n);
}

static struct backtrace_callbacks callbacks = {
    .begin = mem_tracer_begin,
    .callback = mem_tracer_entry,
    .end = mem_tracer_end
};

rbtree_compare_result core_record_ip_compare(struct record_node *ln, 
    struct record_node *rn) {
    return (long)((intptr_t)ln->ipkey - (intptr_t)rn->ipkey);
}

int core_record_backtrace(struct record_class *rc, struct record_node *node) {
    return backtrace_extract_path(&rc->tracer, &callbacks, node);
}

void core_record_print_path(struct record_class *path, struct record_node *node, 
    const struct printer *vio, const char *separator) {
    struct ip_array ips;
    char str[1024];

    ips.ip = ip_first(&node->ipr);
    ips.n = ip_size(&node->ipr);
    int ret = backtrace_transform_path(&path->tracer, &ips, str, sizeof(str));
    if (ret > 0)
        virt_print(vio, "%s", str);
}

struct record_node *core_record_node_allocate(struct record_class *rc, 
    size_t max_depth) {
    ASSERT_TRUE(rc->node_size >= sizeof(struct record_node));
    size_t node_size = (rc->node_size + (sizeof(void *) - 1)) & ~(sizeof(void *) - 1);
    struct record_node *rn = memory_allocate(rc->allocator, 
        node_size + max_depth * sizeof(void *), NULL);
    if (rn != NULL) {
        memset(rn, 0, rc->node_size);
        rbtree_set_off_tree(&rn->node);
        rn->ipr.max_depth = max_depth;
        rn->ipr.ip = (void **)((char *)rn + node_size);
        rn->ipr.sp = max_depth;
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
        node->ipkey = ipkey_generate(0, (void*)ip_first(&node->ipr), 
            ip_size(&node->ipr) * sizeof(void *));
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
