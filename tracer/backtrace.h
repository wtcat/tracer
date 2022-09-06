/*
 * Copyright 2022 wtcat
 */
#ifndef TRACE_BACKTRACE_H_
#define TRACE_BACKTRACE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

#define BACKTRACE_MAX_LIMIT 256

#ifndef __defined_backtrace_entry_t__
#define __defined_backtrace_entry_t__
struct backtrace_entry {
    const char *symbol;
    const char *filename;
    unsigned long pc;
    int line;
};
typedef struct backtrace_entry backtrace_entry_t;
#endif /* backtrace_entry_t */

struct backtrace_callbacks {
    void (*begin)(void *user);
    void (*callback)(backtrace_entry_t *entry, void *user);
    void (*end)(void *user);
    int skip_level;
}; 

struct backtrace_class {
    void (*backtrace)(struct backtrace_class *cls, 
        struct backtrace_callbacks *cb, void *user);
    void *context;
    int min_limit;
    int max_limit;
};

struct backtrace_class *backtrace_get_instance(void);

static inline void do_backtrace(struct backtrace_class *path, 
    struct backtrace_callbacks *cb, void *user) {
    path->backtrace(path, cb, user);
}

static inline void backtrace_set_context(struct backtrace_class *path, 
    void *context) {
    path->context = context;
}

static inline void backtrace_set_limits(struct backtrace_class *cls, 
    int min_limit, int max_limit) {
    if (!max_limit)
        max_limit = BACKTRACE_MAX_LIMIT;
    cls->min_limit = min_limit;
    cls->max_limit = max_limit;
}

#ifdef __cplusplus
}
#endif
#endif // TRACE_BACKTRACE_H_
