/*
 * Copyright 2022 wtcat
 */
#ifndef TRACE_BACKTRACE_H_
#define TRACE_BACKTRACE_H_

#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C"{
#endif

#define BACKTRACE_MAX_LIMIT 64
#define BACKTRACE_SEPARATOR_SIZE 16

struct backtrace_class;

enum bracktrace_type {
    FAST_BACKTRACE,
    UNWIND_BACKTRACE
};

struct backtrace_entry {
    void **ip;
    size_t n;
};

struct ip_record {
    size_t sp;
    size_t max_depth;
    void **ip;
};

struct ip_array {
    void **ip;
    size_t n;
};

struct backtrace_callbacks {
    void (*begin)(struct backtrace_class *cls, void *user);
    void (*end)(struct backtrace_class *cls, void *user, int err);
    void (*callback)(const struct backtrace_entry *entry, void *user);
}; 

struct backtrace_class {
    int (*backtrace)(struct backtrace_class *cls, struct backtrace_callbacks *cb, void *user);
    ssize_t (*transform)(struct backtrace_class *cls, void *ip, char *buf, size_t maxlen);
    int (*transform_prepare)(struct backtrace_class *cls);
    void (*transform_post)(struct backtrace_class *cls);
    char separator[BACKTRACE_SEPARATOR_SIZE];
    int min_limit;
    int max_limit;
    size_t ctx_size;
    void *context;
};

static inline int ip_copy(struct ip_record *node, void *const ip[], size_t n) {
    if (node == NULL || ip == NULL)
        return -EINVAL;
    size_t size = (node->sp < n)? node->sp: n;
    size_t i = 0, sp = node->sp;
    while (size > 0) {
        node->ip[sp - i - 1] = (void *)ip[i];
        i++;
        size--;
    }
    node->sp = sp - i;
    return 0;
}

static inline size_t ip_size(const struct ip_record *n) {
    return n->max_depth - n->sp;
}

static inline void **ip_first(const struct ip_record *n) {
    return (void **)(n->ip + n->sp);
}

static inline void user_backtrace_entry(struct backtrace_callbacks *cb, 
    const struct backtrace_entry *entry, void *user) {
    if (cb->callback)
        cb->callback(entry, user);
}

static inline int do_backtrace(struct backtrace_class *cls, 
    struct backtrace_callbacks *cb, void *user) {
    return cls->backtrace(cls, cb, user);
}

static inline size_t backtrace_context_size(struct backtrace_class *cls) {
    return cls->ctx_size;
}

int backtrace_set_path_window(struct backtrace_class *cls, 
    int min_limit, int max_limit);
int backtrace_set_path_separator(struct backtrace_class *tracer, const char *separator);
int backtrace_extract_path(struct backtrace_class *tracer, 
    struct backtrace_callbacks *cb, void *user);
ssize_t backtrace_transform_path(struct backtrace_class *tracer, struct ip_array *ips, 
    char *buffer, size_t maxlen);
int backtrace_init(enum bracktrace_type type, struct backtrace_class *cls);

#ifdef __cplusplus
}
#endif
#endif // TRACE_BACKTRACE_H_
