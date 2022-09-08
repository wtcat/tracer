/*
 * Copyright 2022 wtcat
 */
#ifndef TRACE_BACKTRACE_H_
#define TRACE_BACKTRACE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"{
#endif

#define BACKTRACE_MAX_LIMIT 64

enum bracktrace_type {
    FAST_BACKTRACE,
    UNWIND_BACKTRACE
};

#ifndef __defined_backtrace_entry_t__
#define __defined_backtrace_entry_t__
struct backtrace_entry {
    const char *symbol;
    const char *filename;
    unsigned long pc;
    int line;
    void **ip;
    size_t n;
};
typedef struct backtrace_entry backtrace_entry_t;
#endif /* backtrace_entry_t */

struct backtrace_callbacks {
    void (*begin)(void *user);
    void (*callback)(backtrace_entry_t *entry, void *user);
    void (*end)(void *user);
}; 

struct backtrace_class {
    void (*backtrace)(struct backtrace_class *cls, 
        struct backtrace_callbacks *cb, void *user);
    void (*begin)(struct backtrace_class *cls, void *context, bool to_symbol);
    int (*sym_entry)(struct backtrace_class *cls, 
        void *ip, char *sym, size_t max, void *context);
    void (*end)(struct backtrace_class *cls, void *context, bool to_symbol);
    int min_limit;
    int max_limit;
    size_t ctx_size;
};

static inline void do_backtrace(struct backtrace_class *path, 
    struct backtrace_callbacks *cb, void *user) {
    path->backtrace(path, cb, user);
}

static inline int backtrace_addr2symbol(struct backtrace_class *path, 
    void *ip, char *sym, size_t max, void *user) {
    if (path->sym_entry)
        return path->sym_entry(path, ip, sym, max, user);
    return -EINVAL;
}

static inline void backtrace_begin(struct backtrace_class *path, 
    void *context, bool to_symbol) {
    if (path->begin)
        return path->begin(path, context, to_symbol);
}

static inline void backtrace_end(struct backtrace_class *path, 
    void *context, bool to_symbol) {
    if (path->end)
        return path->end(path, context, to_symbol);
}

static inline void backtrace_set_limits(struct backtrace_class *cls, 
    int min_limit, int max_limit) {
    if (!max_limit)
        max_limit = BACKTRACE_MAX_LIMIT;
    cls->min_limit = min_limit;
    cls->max_limit = max_limit;
}

static inline size_t backtrace_context_size(struct backtrace_class *cls) {
    return cls->ctx_size;
}

void backtrace_init(enum bracktrace_type type, struct backtrace_class *cls);

#ifdef __cplusplus
}
#endif
#endif // TRACE_BACKTRACE_H_
