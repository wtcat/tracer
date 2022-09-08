/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <execinfo.h>
#include <libunwind.h>

#include "base/utils.h"
#include "tracer/backtrace.h"

static void unix_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    void *ip_array[BACKTRACE_MAX_LIMIT];
    struct backtrace_entry e;
    unw_cursor_t cursor;
    unw_context_t uc;
    int n = 0;

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    cb->begin(user);
    while (unw_step(&cursor) > 0) {
        unw_word_t ip;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        ip_array[n++] = (void *)ip;
    }
    if (n > cls->min_limit) {
        e.ip = ip_array + cls->min_limit;
        e.n = MIN(cls->max_limit, n - cls->min_limit);
        cb->callback(&e, user);
    }
    cb->end(user);
}

static void fast_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    struct backtrace_entry e;
    void *ip_array[BACKTRACE_MAX_LIMIT*2];
    int min, max;
    int ret;

    cb->begin(user);
    min = cls->min_limit;
    ret = backtrace(ip_array, sizeof(ip_array));
    if (ret > min) {
        max = cls->max_limit;
        e.ip = ip_array + min;
        e.n = MIN(max, ret);
        cb->callback(&e, user);
    }
    cb->end(user);
}

static int fast_symbol_entry(struct backtrace_class *cls, 
    void *ip, char *sym, size_t max, void *context) {
    (void) context;
    char **ss; 
    if (ip == NULL)
        return -EINVAL;
    ss = backtrace_symbols(&ip, 1);
    if (ss) {
        size_t len = strlen(*ss);
        size_t cplen = MIN(max - 1, len);
        strncpy(sym, *ss, cplen);
        sym[cplen] = '\0';
        free(ss);
        return 0;
    }
    return -ENOENT;
}

int backtrace_init(enum bracktrace_type type, struct backtrace_class *cls) {
    if (type == UNWIND_BACKTRACE) {
        cls->backtrace = unix_backtrace;
        cls->sym_entry = NULL;
    } else {
        cls->backtrace = fast_backtrace;
        cls->sym_entry = fast_symbol_entry;
    }
    cls->min_limit = 1;
    cls->max_limit = BACKTRACE_MAX_LIMIT;
    return 0;
}
