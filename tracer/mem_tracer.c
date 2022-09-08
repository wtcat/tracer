/*
 * Copyright 2022 wtcat
 */
#include "base/allocator.h"
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(_MSC_VER)
#include <threads.h>
#endif
#include "base/list.h"
#include "base/utils.h"
#include "base/printer.h"
#include "tracer/tracer_core.h"
#include "tracer/mem_tracer.h"
#include "tracer/backtrace.h"

#if !defined(_MSC_VER)
#define MUTEX_LOCK_DECLARE(name) mtx_t name
#define MUTEX_INIT(_path) \
    mtx_init(&(_path)->lock, mtx_plain)
#define MUTEX_LOCK(_path) \
    mtx_lock(&(_path)->lock)
#define MUTEX_UNLOCK(_path) \
    mtx_unlock(&(_path)->lock)
#define MUTEX_DEINIT(_path) \
    (void) (_path)
#else
#include <windows.h>
#define MUTEX_LOCK_DECLARE(name) HANDLE name
#define MUTEX_INIT(_path) \
    (_path)->lock = CreateMutex(NULL, FALSE, NULL)
#define MUTEX_LOCK(_path) \
    WaitForSingleObject((_path)->lock, INFINITE)
#define MUTEX_UNLOCK(_path) \
    ReleaseMutex((_path)->lock)
#define MUTEX_DEINIT(_path) \
    CloseHandle((_path)->lock)
#endif
    
enum node_type {
    IDLE_NODE   = 0,
    MEMB_NODE,
    HEAD_NODE
};

struct iter_argument {
    const struct printer *vio;
    struct path_class *path;
    size_t msize;
};

struct path_class {
#define PATH_SEPARATOR_SIZE 16
    struct record_class base;
    struct record_tree tree;
    struct backtrace_class tracer;
    MUTEX_LOCK_DECLARE(lock);
    size_t path_size;
    char separator[PATH_SEPARATOR_SIZE];
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
    void *context; //For path_class::tracer
};

_Static_assert(sizeof(struct path_class) <= MTRACER_INST_SIZE, "Over size");

static const char mdump_info[] = {
"\n\n******************************************************\n"
    "*                  Memory Tracer Dump                *\n"
    "******************************************************\n"
};

static RBTree_Compare_result sum_compare(const RBTree_Node *a,
    const RBTree_Node *b) {
    struct mem_record_node *p1 = CONTAINER_OF(a, struct mem_record_node, rbnode);
    struct mem_record_node *p2 = CONTAINER_OF(b, struct mem_record_node, rbnode);
    return core_record_ip_compare(&p1->base, &p2->base);
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

static int mem_instert(struct path_class *path, struct mem_record_node *node) {
    RBTree_Node *found;
    if (path == NULL || node == NULL)
        return -EINVAL;
    found = rbtree_insert(&path->tree.root, &node->rbnode, path->tree.compare, true);
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

static void _backtrace_begin(void *user) {
    struct path_class *path = (struct path_class *)user;
    struct record_class *rc = &path->base;
    struct mem_record_node *node = (struct mem_record_node *)rc->pnode;
    // puts("Backtrace Begin:\n");
    backtrace_begin(&path->tracer, node->context, false);
}

static void _backtrace_entry(struct backtrace_entry *entry, void *user) {
    struct path_class *path = (struct path_class *)user;
    struct record_class *rc = &path->base;
    struct mem_record_node *node = (struct mem_record_node *)rc->pnode;
    core_record_copy_ip(&node->base, (const void **)entry->ip, entry->n);
}

static void _backtrace_end(void *user) {
    struct path_class *path = (struct path_class *)user;
    struct record_class *rc = &path->base;
    struct mem_record_node *mrn = (struct mem_record_node *)rc->pnode; 
    backtrace_end(&path->tracer, mrn->context, false);
    core_record_add(rc, &mrn->base);
    mem_instert(path, mrn);
}

static void mem_path_print(const struct printer *vio, struct path_class *path, 
    struct mem_record_node *node, const char *separator) {
    size_t n = core_record_ip_size(&node->base);
    void **ip = core_record_ip(&node->base);
    char str[256];

    virt_print(vio, "<Path>: ");
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
    virt_print(vio, "\n");
}

static bool iterator(struct record_node *n, void *u) {
    struct iter_argument *ia = (struct iter_argument *)u;
    const struct printer *vio = ia->vio;
    struct mem_record_node *mrn = CONTAINER_OF(n, struct mem_record_node, base);
    ia->msize += mrn->size;
    mem_path_print(vio, ia->path, mrn, ia->path->separator);
    virt_print(vio, "\tMemory: 0x%p Size: %ld\n", 
        mrn->ptr, mrn->size);
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
    mem_path_print(vio, ia->path, hnode, ia->path->separator);
    virt_print(vio, "\tMemory: 0x%p Size: %ld\n", 
        hnode->ptr, hnode->size);
    list_for_each(pos, &hnode->head) {
        struct mem_record_node *p = CONTAINER_OF(pos, struct mem_record_node, node);
        sum += p->size;
        virt_print(vio, "\tMemory: 0x%p Size: %ld\n", p->ptr, p->size);
    }
    virt_print(vio, " \tMemory Used: %u B (%.2f KB)\n",  
        sum, (float)sum / 1024);
    ia->msize += sum;
    return false;
}

static void *mem_alloc(struct mem_allocator *m, size_t size) {
    (void) m;
    return malloc(size);
}

static void mem_free(struct mem_allocator *m, void *ptr) {
    (void) m;
    free(ptr);
}

static struct mem_allocator allocator = {
    .allocate = mem_alloc,
    .free = mem_free
};

static struct backtrace_callbacks callbacks = {
    .begin = _backtrace_begin,
    .callback = _backtrace_entry,
    .end = _backtrace_end
};

void mem_tracer_set_path_length(void *context, size_t maxlen) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    if (!maxlen)
        maxlen = 1;
    MUTEX_LOCK(path);
    path->path_size = maxlen;
    MUTEX_UNLOCK(path);
}

void *mem_tracer_alloc(void *context, size_t size) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    MUTEX_LOCK(path);
    void *ptr = memory_allocate(path->base.allocator, size);
    if (ptr) {
        struct mem_record_node *mrn;
        mrn = (struct mem_record_node *)core_record_node_allocate(&path->base, 
            path->path_size + backtrace_context_size(&path->tracer));
        mrn->rbnode.color = RBT_RED;
        mrn->ptr = ptr;
        mrn->size = size;
        mrn->context = mrn + 1;
        path->base.pnode = mrn;
        do_backtrace(&path->tracer, &callbacks, path);
    }
    MUTEX_UNLOCK(path);
    return ptr;
}

void mem_tracer_free(void *context, void *ptr) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    struct mem_record_node *rn;
    assert(ptr != NULL);
    MUTEX_LOCK(path);
    memory_free(path->base.allocator, ptr);
    rn = mem_find(&path->base, ptr);
    if (rn) {
        if (rn->ntype == HEAD_NODE) {
            rbtree_extract(&path->tree.root, &rn->rbnode);
            if (!list_empty(&rn->head)) {
                struct mem_record_node *new_node;
                new_node = CONTAINER_OF(rn->head.next, struct mem_record_node, node);
                list_del(&rn->head);
                mem_instert(path, new_node);
            }
        } else if (rn->ntype == MEMB_NODE)
            list_del(&rn->node);
        core_record_del(&path->base, &rn->base);
    }
    MUTEX_UNLOCK(path);
}

void mem_tracer_dump(void *context, const struct printer *vio, enum mdump_type type) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    struct iter_argument ia = {0};
    time_t now;

    MUTEX_LOCK(path);
    virt_print(vio, mdump_info);
    if (type == MEM_SORTED_DUMP) {
        ia.vio = vio;
        ia.path = path;
        rbtree_iterate(&path->tree.root, RBT_RIGHT, sorted_iterator, &ia);
        goto _print;
    }
    if (type == MEM_SEQUEUE_DUMP) {
        ia.vio = vio;
        ia.path = path;
        core_record_visitor(&path->base, iterator, &ia);
    }
_print:
    time(&now);
    virt_print(vio, "\nTotal Used: %u B (%.2f KB)\n", 
        ia.msize, (float)ia.msize/1024);
    virt_print(vio, "Time: %s\n\n", asctime(localtime(&now)));
    MUTEX_UNLOCK(path);
}

int mem_tracer_set_allocator(void *context, struct mem_allocator *alloc) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    if (alloc == NULL)
        return -EINVAL;
    MUTEX_LOCK(path);
    path->base.allocator = alloc;
    MUTEX_UNLOCK(path);
    return 0;
}

int mem_tracer_set_path_separator(void *context, const char *separator) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    if (separator == NULL)
        return -EINVAL;
    MUTEX_LOCK(path);
    memset(path->separator, 0, PATH_SEPARATOR_SIZE);
    strncpy(path->separator, separator, PATH_SEPARATOR_SIZE-1);
    MUTEX_UNLOCK(path);
    return 0;
}

void mem_tracer_destory(void *context) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    MUTEX_LOCK(path);
    core_record_destroy(&path->base);
    MUTEX_UNLOCK(path);
}

void mem_tracer_init(void *context) {
    assert(context != NULL);
    struct path_class *path = (struct path_class *)context;
    memset(path, 0, sizeof(*path));
    path->base.tree.compare = ptr_compare;
    INIT_LIST_HEAD(&path->base.head);
    path->base.allocator = &allocator;
    path->base.node_size = sizeof(struct mem_record_node);
    path->tree.compare = sum_compare;
    path->path_size = BACKTRACE_MAX_LIMIT;
    path->separator[0] = '/';
    backtrace_init(FAST_BACKTRACE, &path->tracer);
    MUTEX_INIT(path);
}

void mem_tracer_deinit(void* context) {
    struct path_class* path = (struct path_class*)context;
    mem_tracer_destory(context);
    MUTEX_DEINIT(path);
}

void mem_tracer_set_path_limits(void *context, int min, int max) {
    struct path_class* path = (struct path_class*)context;
    MUTEX_LOCK(path);
    backtrace_set_limits(&path->tracer, min, max);
    MUTEX_UNLOCK(path);
}
