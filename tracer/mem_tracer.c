/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "base/list.h"
#include "base/utils.h"
#include "base/printer.h"
#include "tracer/tracer_core.h"
#include "tracer/mem_tracer.h"
#include "tracer/backtrace.h"

enum node_type {
    IDLE_NODE   = 0,
    MEMB_NODE,
    HEAD_NODE
};

struct iter_argument {
    const struct printer *vio;
    size_t msize;
};

struct path_container {
    struct record_tree tree;
    size_t path_size;
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

static void backtrace_begin(void *user) {
    // puts("Backtrace Begin:\n");
}

static void backtrace_entry(struct backtrace_entry *entry, void *user) {
    struct record_class *rc = (struct record_class *)user;
    struct mem_record_node *node = (struct mem_record_node *)rc->pnode;
    node->line = entry->line;
    mem_path_append(&node->base, entry->symbol);
}

static void backtrace_end(void *user) {
    struct record_class *rc = (struct record_class *)user;
    struct mem_record_node *mrn = (struct mem_record_node *)rc->pnode; 
    core_record_add(rc, &mrn->base);
    mem_instert((struct path_container *)rc->user, mrn);
}

static bool iterator(struct record_node *n, void *u) {
    struct iter_argument *ia = (struct iter_argument *)u;
    const struct printer *vio = ia->vio;
    struct mem_record_node *mrn = CONTAINER_OF(n, struct mem_record_node, base);
    ia->msize += mrn->size;
    virt_print(vio, "CallPath: %s\n\tMemory: 0x%p Size: %ld\n", 
        mrn->base.path, mrn->ptr, mrn->size);
    return true;
}

static bool sorted_iterator(const RBTree_Node *node, RBTree_Direction dir, void *arg) {
    struct iter_argument *ia = (struct iter_argument *)arg;
    const struct printer *vio = ia->vio;
    size_t sum = 0;
    (void) dir;
    struct list_head *pos;
    struct mem_record_node *hnode = CONTAINER_OF(node, struct mem_record_node, rbnode);
    sum += hnode->size;
    virt_print(vio, "CallPath: %s\n\tMemory: 0x%p Size: %ld\n", 
        hnode->base.path, hnode->ptr, hnode->size);
    list_for_each(pos, &hnode->head) {
        struct mem_record_node *p = CONTAINER_OF(pos, struct mem_record_node, node);
        sum += hnode->size;
        virt_print(vio, "\tMemory: 0x%p Size: %ld\n", p->ptr, p->size);
    }
    ia->msize += sum;
    return false;
}

static struct backtrace_callbacks callbacks = {
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
    .path_size = 512 
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

void mem_tracer_set_path_length(size_t maxlen) {
    mem_container.path_size = maxlen;
}

void *mem_tracer_alloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        struct mem_record_node *mrn;
        mrn = (struct mem_record_node *)record_node_allocate(&container, 0, mem_container.path_size);
        mrn->rbnode.color = RBT_RED;
        mrn->ptr = ptr;
        mrn->size = size;
        container.pnode = mrn;
        do_backtrace(backtrace_get_instance(), &callbacks, &container);
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
    struct iter_argument ia = {0};
    time_t now;
    virt_print(vio, 
        "\n\n******************************************************\n"
            "*                  Memory Tracer Dump                *\n"
            "******************************************************\n"
    );
    if (type == MEM_SORTED_DUMP) {
        struct path_container *pc = container.user;
        ia.vio = vio;
        rbtree_iterate(&pc->tree.root, RBT_RIGHT, sorted_iterator, &ia);
        goto _print;
    }
    if (type == MEM_SEQUEUE_DUMP) {
        ia.vio = vio;
        core_record_visitor(&container, iterator, &ia);
    }
_print:
    time(&now);
    virt_print(vio, "Memory Used: %u B (%.2f KB)\n", 
        ia.msize, (float)ia.msize/1024);
    virt_print(vio, "Time: %s\n\n", asctime(localtime(&now)));
}

void mem_tracer_destory(void) {
    core_record_destroy(&container);
}
