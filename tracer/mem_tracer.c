/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <stdbool.h>
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
#include "base/allocator.h"
#include "base/assert.h"
#include "base/backtrace.h"
#include "tracer/tracer_core.h"
#include "tracer/mem_tracer.h"


/* For thread safe */
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

/* For memory overflow check */
typedef size_t _mem_fill_t; 
struct prot_mem {
    _mem_fill_t magic;
    _mem_fill_t size;
    char buffer[];
#define PROTMEM_MAGIC 0xDEADBEEF
#define PROTMEM_ALIGN_SIZE(_size) \
    (((_size) + (sizeof(_mem_fill_t) - 1)) & ~(sizeof(_mem_fill_t) - 1))
#define PROTMEM_GET_SIZE(_size) \
    (PROTMEM_ALIGN_SIZE(_size) + sizeof(struct prot_mem) + sizeof(_mem_fill_t))
#define PROTMEM_WRITE(_ptr, _ofs, _v) \
    *(_mem_fill_t *)((char *)(_ptr) + (_ofs)) = (_v)
#define PROTMEM_READ(_ptr, _ofs) \
    *(_mem_fill_t *)((char *)(_ptr) + (_ofs))
};

struct mem_argument {
    struct path_class *path;
    union {
        struct mem_record_node *mnode;
        size_t msize;
    };
};

struct path_class {
#define PATH_SEPARATOR_SIZE 16
    struct record_class base;
    struct record_tree tree;
    struct mem_allocator *allocator;
    const struct printer *vio;
    MUTEX_LOCK_DECLARE(lock);
    size_t path_size;
    char separator[PATH_SEPARATOR_SIZE];
    unsigned int options;
};

struct mem_record_node {
    struct record_node base;
    rbtree_node rbnode;
    union {
        struct list_head head;
        struct list_head node;
    };
    void  *ptr;
    size_t size;
};

_Static_assert(sizeof(struct path_class) <= MTRACER_INST_SIZE, "Over size");
static struct printer mem_printer;
static const char mdump_info[] = {
"\n\n******************************************************\n"
    "*                  Memory Tracer Dump                *\n"
    "******************************************************\n"
};

static rbtree_compare_result sum_compare(const rbtree_node *a,
    const rbtree_node *b) {
    ASSERT_TRUE(a != NULL);
    ASSERT_TRUE(b != NULL);
    struct mem_record_node *p1 = CONTAINER_OF(a, struct mem_record_node, rbnode);
    struct mem_record_node *p2 = CONTAINER_OF(b, struct mem_record_node, rbnode);
    return core_record_ip_compare(&p1->base, &p2->base);
}

static rbtree_compare_result ptr_compare(const rbtree_node *a,
    const rbtree_node *b) {
    struct mem_record_node *p1 = CONTAINER_OF(a, struct mem_record_node, base.node);
    struct mem_record_node *p2 = CONTAINER_OF(b, struct mem_record_node, base.node);
    return (long)p1->ptr - (long)p2->ptr;
}

static struct mem_record_node *mem_find(struct record_class *rc, void *ptr) {
    rbtree_node *found;
    if (rc == NULL || ptr == NULL)
        return NULL;
    struct mem_record_node mrn;
    mrn.ptr = ptr;
    found = rbtree_find(&rc->tree.root, &mrn.base.node, rc->tree.compare, true);
    if (found)
        return CONTAINER_OF(found, struct mem_record_node, base.node);
    return NULL;
}

static int mem_instert(struct path_class *path, struct mem_record_node *node, bool reset) {
    rbtree_node *found;
    if (path == NULL || node == NULL)
        return -EINVAL;
    found = rbtree_insert(&path->tree.root, &node->rbnode, path->tree.compare, true);
    if (found) {
        struct mem_record_node *hnode = CONTAINER_OF(found, struct mem_record_node, rbnode);
        list_add_tail(&node->node, &hnode->head);
    } else if (reset) {
        INIT_LIST_HEAD(&node->head);
    }
    return 0;
}

static inline void mem_node_insert(struct path_class *path, struct mem_record_node *mrn) {
        core_record_add(&path->base, &mrn->base);
        mem_instert(path, mrn, true);
}

static void mem_path_print(struct path_class *path, struct mem_record_node *node, 
    const char *separator) {
    const struct printer *vio = path->vio;
    virt_print(vio, "<Path>: ");
    core_record_print_path(&path->base, &node->base, vio, path->separator);
    virt_print(vio, "\n");
}

static bool free_iterator(struct record_node *n, void *u) {
    struct path_class *path = (struct path_class *)u;
    struct mem_record_node *mrn = CONTAINER_OF(n, 
        struct mem_record_node, base);
    if (mrn->ptr) {
        memory_free(path->allocator, mrn->ptr, mrn);
        mrn->ptr = NULL;
    }
    return true;
}

static bool iterator(struct record_node *n, void *u) {
    struct mem_argument *ia = (struct mem_argument *)u;
    const struct printer *vio = ia->path->vio;
    struct mem_record_node *mrn = CONTAINER_OF(n, struct mem_record_node, base);
    ia->msize += mrn->size;
    if (vio != NULL) {
        mem_path_print(ia->path, mrn, ia->path->separator);
        virt_print(vio, "\tMemory: 0x%p Size: %ld\n",
            mrn->ptr, mrn->size);
    }
    return true;
}

static bool sorted_iterator(const rbtree_node *node, void *arg) {
    struct mem_argument *ia = (struct mem_argument *)arg;
    const struct printer *vio = ia->path->vio;
    size_t sum = 0;
    struct list_head *pos;
    struct mem_record_node *hnode = CONTAINER_OF(node, struct mem_record_node, rbnode);
    sum += hnode->size;
    mem_path_print(ia->path, hnode, ia->path->separator);
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

static void mem_overflow_dump(struct mem_argument *ia) {
    struct mem_record_node *killer, *victim;
    const struct printer *vio = ia->path->vio;
    victim = ia->mnode;
    killer = (struct mem_record_node *)core_record_lessthen_node(&ia->mnode->base);
    virt_print(vio, "\n\n@Victime vs @Killer {\n");
    mem_path_print(ia->path, victim, ia->path->separator);
    mem_path_print(ia->path, killer, ia->path->separator);
    virt_print(vio, "\n}\n");
}

static void *protmem_alloc(struct mem_allocator *m, size_t size, void *user) {
    struct prot_mem *ptr;
    size_t rsize = PROTMEM_GET_SIZE(size);
    ptr = (struct prot_mem *)memory_allocate(m->context, rsize, user);
    if (ptr) {
        size_t aligned_size = PROTMEM_ALIGN_SIZE(size);
        ptr->magic = PROTMEM_MAGIC;
        ptr->size = aligned_size;
        PROTMEM_WRITE(ptr->buffer, aligned_size, PROTMEM_MAGIC);
        return ptr->buffer;
    }
    return NULL;
}

static void protmem_free(struct mem_allocator *m, void *p, void *user) {
    struct prot_mem *ptr;
    if (p == NULL)
        return;
    ptr = CONTAINER_OF(p, struct prot_mem, buffer);
    if (ptr->magic == PROTMEM_MAGIC) {
        size_t aligned_size = PROTMEM_ALIGN_SIZE(ptr->size);
        _mem_fill_t magic = PROTMEM_READ(ptr->buffer, aligned_size);
        if (magic == PROTMEM_MAGIC) {
            memory_free(m->context, ptr, NULL);
            return;
        }
    }
    memory_free(m->context, ptr, user);
    if (user != NULL)
        mem_overflow_dump((struct mem_argument *)user);
}

static void *mem_alloc(struct mem_allocator *m, size_t size, void *user) {
    (void) m;
    (void) user;
    return malloc(size);
}

static void mem_free(struct mem_allocator *m, void *ptr, void *user) {
    (void) m;
    (void) user;
    free(ptr);
}

static struct mem_allocator allocator = {
    .allocate = mem_alloc,
    .free = mem_free
};

static struct mem_allocator protmem_allocator = {
    .allocate = protmem_alloc,
    .free = protmem_free,
    .context = NULL
};

static inline struct mem_record_node *mem_node_alloc(struct path_class *path, 
    size_t path_size) {
    return (struct mem_record_node *)core_record_node_allocate(&path->base, 
        path_size);
}

static struct mem_record_node *mem_node_create(struct path_class *path, void *ptr, 
    size_t size) {
    struct mem_record_node *mnode = mem_node_alloc(path, path->path_size);
    if (mnode) {
        rbtree_set_off_tree(&mnode->rbnode);
        mnode->ptr = ptr;
        mnode->size = size;
        return mnode;
    }
    return NULL;
}

static int mem_node_delete(struct path_class *path, struct mem_record_node *rn) {
    if (!rbtree_is_node_off_tree(&rn->rbnode)) {
        rbtree_extract(&path->tree.root, &rn->rbnode);
        if (!list_empty(&rn->head)) {
            struct mem_record_node *new_node;
            new_node = CONTAINER_OF(rn->head.next, 
                struct mem_record_node, node);
            list_del(&rn->head);
            mem_instert(path, new_node, false);
        }
    } else {
        list_del(&rn->node);
    }
    core_record_del(&path->base, &rn->base);
    return 0;
}

void mem_tracer_set_path_length(void *context, size_t maxlen) {
    ASSERT_TRUE(context != NULL);
    struct path_class *path = (struct path_class *)context;
    if (!maxlen)
        maxlen = 1;
    MUTEX_LOCK(path);
    path->path_size = maxlen;
    MUTEX_UNLOCK(path);
}

void *mem_tracer_alloc(void *context, size_t size) {
    ASSERT_TRUE(context != NULL);
    struct path_class *path = (struct path_class *)context;
    struct mem_record_node *mnode;
    MUTEX_LOCK(path);
    void *ptr = memory_allocate(path->allocator, size, NULL);
    if (ptr) {
        mnode = mem_node_create(path, ptr, size);
        ASSERT_TRUE(mnode != NULL);
        core_record_backtrace(&path->base, &mnode->base);
        mem_node_insert(path, mnode);
    }
    MUTEX_UNLOCK(path);
    return ptr;
}

void mem_tracer_free(void *context, void *ptr) {
    ASSERT_TRUE(context != NULL);
    struct path_class *path = (struct path_class *)context;
    struct mem_record_node *rn;
    ASSERT_TRUE(ptr != NULL);
    MUTEX_LOCK(path);
    rn = mem_find(&path->base, ptr);
    if (rn) {
        memory_free(path->allocator, ptr, rn);
        mem_node_delete(path, rn);
    } else if (path->options & MEM_CHECK_INVALID) {
        virt_print(path->vio, "Error***: Free invalid pointer (%p):\n", ptr);
        mem_path_print(path, rn, path->separator);
    }
    MUTEX_UNLOCK(path);
}

void mem_tracer_dump(void *context, enum mem_dumper type) {
    ASSERT_TRUE(context != NULL);
    struct path_class *path = (struct path_class *)context;
    struct mem_argument ia = {0};
    time_t now;
    MUTEX_LOCK(path);
    const struct printer *vio = path->vio;
    virt_print(vio, mdump_info);
    if (type == MEM_DUMP_SORTED) {
        ia.path = path;
        rbtree_iterate(&path->tree.root, sorted_iterator, &ia);
        goto _print;
    }
    if (type == MEM_DUMP_SEQUENCE) {
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

int mem_tracer_set_path_separator(void *context, const char *separator) {
    ASSERT_TRUE(context != NULL);
    struct path_class *path = (struct path_class *)context;
    if (separator == NULL)
        return -EINVAL;
    MUTEX_LOCK(path);
    memset(path->separator, 0, PATH_SEPARATOR_SIZE);
    strncpy(path->separator, separator, PATH_SEPARATOR_SIZE-1);
    MUTEX_UNLOCK(path);
    return 0;
}

void mem_tracer_set_path_limits(void *context, int min, int max) {
    ASSERT_TRUE(context != NULL);
    struct path_class* path = (struct path_class*)context;
    MUTEX_LOCK(path);
    backtrace_set_limits(&path->base.tracer, min, max);
    MUTEX_UNLOCK(path);
}

void mem_tracer_set_printer(void *context, const struct printer *vio) {
    ASSERT_TRUE(context != NULL);
    if (vio) {
        struct path_class* path = (struct path_class*)context;
        MUTEX_LOCK(path);
        path->vio = vio;
        MUTEX_UNLOCK(path);
    }
}

size_t mem_tracer_get_used(void* context) {
    ASSERT_TRUE(context != NULL);
    struct path_class* path = (struct path_class*)context;
    struct mem_argument ia = {0};
    MUTEX_LOCK(path);
    ia.path = path;
    core_record_visitor(&path->base, iterator, &ia);
    MUTEX_UNLOCK(path);
    return ia.msize;
}

void mem_tracer_init(void *context, struct mem_allocator *alloc, 
    unsigned int options) {
    ASSERT_TRUE(context != NULL);
    struct path_class *path = (struct path_class *)context;
    memset(path, 0, sizeof(*path));
    MUTEX_INIT(path);
    MUTEX_LOCK(path);
    if (alloc == NULL)
        alloc = &allocator;
    INIT_LIST_HEAD(&path->base.head);
    if (options & MEM_CHECK_OVERFLOW) {
        protmem_allocator.context = alloc;
        path->allocator = &protmem_allocator;
    } else {
        path->allocator = alloc;
    }
    path->base.allocator = alloc;
    path->base.tree.compare = ptr_compare;
    path->base.node_size = sizeof(struct mem_record_node);
    path->tree.compare = sum_compare;
    path->path_size = BACKTRACE_MAX_LIMIT;
    path->separator[0] = '/';
    path->options = options;
    path->vio = &mem_printer;
    printf_printer_init(&mem_printer);
    backtrace_init(FAST_BACKTRACE, &path->base.tracer);
    MUTEX_UNLOCK(path);
}

void mem_tracer_destory(void *context) {
    ASSERT_TRUE(context != NULL);
    struct path_class *path = (struct path_class *)context;
    MUTEX_LOCK(path);
    core_record_visitor(&path->base, free_iterator, path);
    core_record_destroy(&path->base);
    MUTEX_UNLOCK(path);
}

void mem_tracer_deinit(void* context) {
    struct path_class* path = (struct path_class*)context;
    mem_tracer_destory(context);
    MUTEX_DEINIT(path);
}
