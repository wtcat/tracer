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

static uint32_t chksum_generate(uint32_t crc, const uint8_t *data, size_t len) {
	static const uint32_t table[16] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
	};
	crc = ~crc;
	for (size_t i = 0; i < len; i++) {
		uint8_t byte = data[i];
		crc = (crc >> 4) ^ table[(crc ^ byte) & 0x0f];
		crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)byte >> 4)) & 0x0f];
	}
	return ~crc;
}

static void record_chksum_generate(struct record_node *rn) {
    size_t len = strlen(rn->path);
    rn->chksum = chksum_generate(0, (const uint8_t *)rn->path, len);
}

struct record_node *record_node_allocate(struct record_class *rc, 
    size_t symsize, size_t btsize) {
    assert(rc->node_size >= sizeof(struct record_node));
    struct record_node *rn = rc->alloc(rc->node_size + symsize + btsize + 2);
    if (rn != NULL) {
        memset(rn, 0, rc->node_size);
        char *buf = (char *)rn + rc->node_size;
        buf[0] = '\0';
        buf[symsize] = '\0';
        rn->node.color = RBT_RED;
        rn->sym = buf;
        rn->path_ss_ = buf + symsize;
        rn->path = rn->path_ss_ + btsize;
        rn->path[0] = '\0';
    }
    return rn;
}

int mem_path_append(struct record_node *node, const char *str) {
    if (node == NULL || str == NULL)
        return -EINVAL;
    size_t len = strlen(str);
    const char *ss = str + len;
    char *s = node->path;
    while (ss > str && s > node->path_ss_)
        *(--s) = *(--ss);
    if (s > node->path_ss_)
        *(--s) = '/';
    node->path = s;
    return !!str[0];
}

int core_record_add(struct record_class *rc, struct record_node *node) {
    RBTree_Node *found;
    if (rc == NULL || node == NULL)
        return -EINVAL;
    found = rbtree_insert(&rc->tree.root, &node->node, rc->tree.compare, true);
    assert(found == NULL);
    if (!found) {
        record_chksum_generate(node);
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
    rc->free(node);
    return 0;
}

void core_record_destroy(struct record_class *rc) {
    struct list_head *pos, *next;
    list_for_each_safe(pos, next, &rc->head) {
        struct record_node *rn = CONTAINER_OF(pos, struct record_node, link);
        list_del(pos);
        rc->free(rn);
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
