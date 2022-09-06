/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <libunwind.h>

#include "base/list.h"
#include "base/utils.h"
#include "base/printer.h"

#include "tracer/tracer_core.h"
#include "tracer/mem_tracer.h"

enum node_type {
    IDLE_NODE   = 0,
    MEMB_NODE,
    HEAD_NODE
};

struct path_container {
    struct record_tree tree;
};

struct mem_record_node {
    struct record_node base;
    RBTree_Node rbnode;
    union {
        struct list_head head;
        struct list_head node;
    };
    enum node_type ntype;
    void  *ptr;
    size_t size;
    int line;
};

static RBTree_Compare_result sum_compare(const RBTree_Node *a,
    const RBTree_Node *b) {
    struct mem_record_node *p1 = CONTAINER_OF(a, struct mem_record_node, rbnode);
    struct mem_record_node *p2 = CONTAINER_OF(b, struct mem_record_node, rbnode);
    return (long)p1->base.chksum - (long)p2->base.chksum;
}

static RBTree_Compare_result ptr_compare(const RBTree_Node *a,
    const RBTree_Node *b) {
    struct mem_record_node *p1 = CONTAINER_OF(a, struct mem_record_node, base.node);
    struct mem_record_node *p2 = CONTAINER_OF(b, struct mem_record_node, base.node);
    return (long)p1->ptr - (long)p2->ptr;
}

static struct mem_record_node *mem_find(struct record_class *rc, void *ptr) {
    RBTree_Node *found;
    if (rc == NULL || ptr == NULL)
        return NULL;
    struct mem_record_node mrn;
    mrn.ptr = ptr;
    found = rbtree_find(&rc->tree.root, &mrn.base.node, rc->tree.compare, true);
    if (found)
        return CONTAINER_OF(found, struct mem_record_node, base.node);
    return NULL;
}

static int mem_instert(struct path_container *pc, struct mem_record_node *node) {
    RBTree_Node *found;
    if (pc == NULL || node == NULL)
        return -EINVAL;
    found = rbtree_insert(&pc->tree.root, &node->rbnode, pc->tree.compare, true);
    if (found) {
        struct mem_record_node *hnode = CONTAINER_OF(found, struct mem_record_node, rbnode);
        node->ntype = MEMB_NODE;
        list_add_tail(&node->node, &hnode->head);
    } else if (node->ntype != HEAD_NODE) {
        node->ntype = HEAD_NODE;
        INIT_LIST_HEAD(&node->head);
    }
    return 0;
}

static void backtrace(struct backtrace_callbacks *cb, void *user) {
    struct backtrace_entry entry;
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip, offp;
    char name[256];

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    cb->begin(user);
    while (unw_step(&cursor) > 0) {
        name[0] = '\0';
        unw_get_proc_name(&cursor, name, sizeof(name), &offp);
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        entry.symbol = name;
        entry.pc = ip;
        entry.line = -1;
        cb->callback(&entry, user);
    }
    cb->end(user);
}

static void backtrace_begin(void *user) {
    puts("Backtrace Begin:\n");
}

static void backtrace_entry(struct backtrace_entry *entry, void *user) {
    struct record_class *rc = (struct record_class *)user;
    struct mem_record_node *node = (struct mem_record_node *)rc->pnode;
    mem_path_append(&node->base, entry->symbol);
}

static void backtrace_end(void *user) {
    struct record_class *rc = (struct record_class *)user;
    struct mem_record_node *mrn = (struct mem_record_node *)rc->pnode; 
    core_record_add(rc, &mrn->base);
    mem_instert((struct path_container *)rc->user, mrn);
}

static bool iterator(struct record_node *n, void *u) {
    const struct printer *vio = (const struct printer *)u;
    struct mem_record_node *mrn = CONTAINER_OF(n, struct mem_record_node, base);
    virt_print(vio, "CallPath: %s\n\tMemory: 0x%p Size: %ld\n", 
        mrn->base.path, mrn->ptr, mrn->size);
    return true;
}

static bool sorted_iterator(const RBTree_Node *node, RBTree_Direction dir, void *arg) {
    const struct printer *vio = (const struct printer *)arg;
    (void) dir;
    struct list_head *pos;
    struct mem_record_node *hnode = CONTAINER_OF(node, struct mem_record_node, rbnode);
    virt_print(vio, "CallPath: %s\n\tMemory: 0x%p Size: %ld\n", 
        hnode->base.path, hnode->ptr, hnode->size);
    list_for_each(pos, &hnode->head) {
        struct mem_record_node *p = CONTAINER_OF(pos, struct mem_record_node, node);
        virt_print(vio, "\tMemory: 0x%p Size: %ld\n", p->ptr, p->size);
    }
    return false;
}

static struct backtrace_callbacks bt_cbs = {
    .begin = backtrace_begin,
    .callback = backtrace_entry,
    .end = backtrace_end,
    .skip_level = 0
};

static struct path_container mem_container = {
    .tree = {
        .root = RBTREE_INITIALIZER_EMPTY(0),
        .compare = sum_compare
    },    
};

static struct record_class container = {
    .tree = {
        .root = RBTREE_INITIALIZER_EMPTY(0),
        .compare = ptr_compare
    },
    .head = LIST_HEAD_INIT(container.head),
    .alloc = malloc,
    .free = free,
    .node_size = sizeof(struct mem_record_node),
    .user = &mem_container
};

void *mem_tracer_alloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        struct mem_record_node *mrn;
        mrn = (struct mem_record_node *)record_node_allocate(&container, 0, 512);
        mrn->rbnode.color = RBT_RED;
        mrn->ptr = ptr;
        mrn->size = size;
        container.pnode = mrn;
        backtrace(&bt_cbs, &container);
    }
    return ptr;
}

void mem_tracer_free(void *ptr) {
    struct mem_record_node *rn;
    assert(ptr != NULL);
    free(ptr);
    rn = mem_find(&container, ptr);
    if (rn) {
        struct path_container *pc = (struct path_container *)container.user;
        if (rn->ntype == HEAD_NODE) {
            rbtree_extract(&pc->tree.root, &rn->rbnode);
            if (!list_empty(&rn->head)) {
                struct mem_record_node *new_node;
                new_node = CONTAINER_OF(rn->head.next, struct mem_record_node, node);
                list_del(&rn->head);
                mem_instert(pc, new_node);
            }
        } else if (rn->ntype == MEMB_NODE)
            list_del(&rn->node);
        core_record_del(&container, &rn->base);
    }
}

void mem_tracer_dump(const struct printer *vio, enum mdump_type type) {
    switch (type) {
    case MEM_SORTED_DUMP: {
        struct path_container *pc = container.user;
        rbtree_iterate(&pc->tree.root, RBT_RIGHT, sorted_iterator, (void *)vio);
        }
        break;
    case MEM_SEQUEUE_DUMP:
        core_record_visitor(&container, iterator, (void *)vio);
        break;
    default:
        break;
    }
}

void mem_tracer_destory(void) {
    core_record_destroy(&container);
}
