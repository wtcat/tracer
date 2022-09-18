/*
 * Copyright 2022 wtcat
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "base/printer.h"
#include "base/allocator.h"
#include "base/backtrace.h"
#include "base/list.h"


#define PTR_TABLE_SIZE 50
#define TEST_FUN(name) \
    void name(void)

#define TEST_MALLOC(size) \
    do {\
        assert(ptr_index < PTR_TABLE_SIZE); \
        ptr_table[ptr_index++] = mem_tracer_alloc(&mtrace_context, size); \
    } while (0)

#define TEST_FREE() \
    do { \
        for (int i = 0; i < ptr_index; i++) {\
            mem_tracer_free(&mtrace_context, ptr_table[i]); \
            ptr_table[i] = NULL; \
        } \
    } while (0)


// struct ip_data {
// #define IP_MAX_SIZE  12
//     struct ip_record ipr;
//     void *path[IP_MAX_SIZE];
// };

// void ip_node_init(struct ip_data *d) {
//     d->ipr.max_depth = IP_MAX_SIZE;
//     d->ipr.sp = IP_MAX_SIZE;
//     d->ipr.ip = d->path;
// }

// static void tracer_entry(const struct backtrace_entry *entry, void *user) {
//     struct ip_data *d = (struct ip_data *)user;
//     ip_copy(&d->ipr, entry->ip, entry->n);
// }

// static struct backtrace_callbacks callbacks = {
//     .callback = tracer_entry
// };

// int generate_exec_path(struct backtrace_class *tracer, struct ip_data *d) {
//     if (d)
//         return backtrace_extract_path(tracer, &callbacks, d);
//     return -ENOMEM;
// }

// ssize_t transform_exec_path(struct backtrace_class *tracer, struct ip_data *d, 
//     char *buffer, size_t maxlen) {
//     struct ip_array ips;
//     ips.ip = ip_first(&d->ipr);
//     ips.n = ip_size(&d->ipr);
//     return backtrace_transform_path(tracer, &ips, buffer, maxlen);
// }


static int generate_exec_path(void);
static ssize_t transform_exec_path(struct ip_array *ips, char *buffer, size_t maxlen);

TEST_FUN(func_1) {
    generate_exec_path();
}

TEST_FUN(func_2) {
    func_1();
    generate_exec_path();
}

TEST_FUN(func_3) {
    func_2();
    generate_exec_path();
}

TEST_FUN(func_4) {
    func_3();
    generate_exec_path();
    generate_exec_path();
}

TEST_FUN(func_5) {
    func_4();
    generate_exec_path();
}

struct ip_data {
    struct list_head node;
    struct ip_record ipr;
};

struct tracer_struct {
    struct backtrace_class tracer;
    struct list_head head;
    struct ip_data *curr;
};

static bool ip_iterator(struct ip_data *n, void *u) {
    struct ip_array ips;
    char buffer[512];
    ips.ip = ip_first(&n->ipr);
    ips.n = ip_size(&n->ipr);
    transform_exec_path(&ips, buffer, sizeof(buffer));
    printf("<Path>: %s", buffer);
    list_del(&n->node);
    free(n);
    return true;
}

static struct ip_data *ip_node_create(struct backtrace_class *cls) {
    size_t depth = cls->max_limit - cls->min_limit;
    struct ip_data *d = malloc(sizeof(*d) + depth * sizeof(void *));
    if (d) {
        memset(&d->node, 0, sizeof(d->node));
        d->ipr.max_depth = depth;
        d->ipr.sp = depth;
        d->ipr.ip = (void **)(d + 1);
    }
    return d;   
}

static void user_tracer_begin(struct backtrace_class *cls, void *user) {
    (void) cls;
    (void) user;
}

static void user_tracer_end(struct backtrace_class *cls, void *user, int err) {
    struct ip_data *d = (struct ip_data *)user;
    if (!err) {
        struct tracer_struct *ts = CONTAINER_OF(cls, struct tracer_struct, tracer);
        list_add(&d->node, &ts->head);
    } else {
        free(d);
    }
}

static void user_tracer_entry(const struct backtrace_entry *entry, void *user) {
    struct ip_data *d = (struct ip_data *)user;
    ip_copy(&d->ipr, entry->ip, entry->n);
}

static struct tracer_struct tracer_inst;
static struct backtrace_callbacks callbacks = {
    .begin = user_tracer_begin,
    .callback = user_tracer_entry,
    .end = user_tracer_end
};

static int generate_exec_path(void) {
    struct ip_data *d = ip_node_create(&tracer_inst.tracer);
    if (d)
        return backtrace_extract_path(&tracer_inst.tracer, &callbacks, d);
    return -ENOMEM;
}

static ssize_t transform_exec_path(struct ip_array *ips, char *buffer, size_t maxlen) {
    return backtrace_transform_path(&tracer_inst.tracer, ips, buffer, maxlen);
}

static void ip_visitor(struct tracer_struct *ts,
    bool (*iterator)(struct ip_data *n, void *u), 
    void *user) {
    struct list_head *pos, *next;
    list_for_each_safe(pos, next, &ts->head) {
        struct ip_data *d = CONTAINER_OF(pos, struct ip_data, node);
        if (!iterator(d, user))
            break;
    }
}

int main(int argc, char *argv[]) {
    backtrace_init(FAST_BACKTRACE, &tracer_inst.tracer);
    INIT_LIST_HEAD(&tracer_inst.head);
    backtrace_set_path_separator(&tracer_inst.tracer, "\n\t->");
    backtrace_set_path_window(&tracer_inst.tracer, 1, 12);
    
    func_5();
    ip_visitor(&tracer_inst, ip_iterator, NULL);

    return 0;
}