/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <execinfo.h>
#include <libunwind.h>

#include "base/utils.h"
#include "base/backtrace.h"

static int unix_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    void *ip_array[BACKTRACE_MAX_LIMIT];
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip;
    int n = 0;

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    while (unw_step(&cursor) > 0) {
        if (n < cls->min_limit) {
            n++;
            continue;
        }
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        ip_array[n - cls->min_limit] = (void *)ip;
        n++;
    }
    if (n > cls->min_limit) {
        struct backtrace_entry e;
        e.ip = ip_array + cls->min_limit;
        e.n = MIN(cls->max_limit, n - cls->min_limit);
        cb->callback(&e, user);
        return 0;
    }
    return -EINVAL;
}

static ssize_t unix_symbol_transform(struct backtrace_class *cls, void *ip, 
    char *buf, size_t maxlen) {
    unw_cursor_t cursor;
    unw_context_t uc;

    if (ip == NULL)
        return -EINVAL;
    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    if (!unw_get_proc_name(&cursor, buf, maxlen, (unw_word_t *)ip))
        return strlen(buf);
    return -ENOENT;
}

static int unix_transform_prepare(struct backtrace_class *cls) {
    if (cls == NULL)
        return -EINVAL;
    unw_cursor_t *cursor = malloc(sizeof(unw_cursor_t));
    if (cursor) {
        unw_context_t uc;
        unw_getcontext(&uc);
        unw_init_local(cursor, &uc);
        cls->context = cursor;
        return 0;
    }
    return -ENOENT;
}

static void unix_transform_post(struct backtrace_class *cls) {
    if (cls == NULL)
        return;
    if (cls->context) {
        free(cls->context);
        cls->context = NULL;
    }
}

static int fast_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    struct backtrace_entry e;
    void *ip_array[BACKTRACE_MAX_LIMIT*2];
    int min = cls->min_limit;
    int ret = backtrace(ip_array, sizeof(ip_array));
    if (ret > min) {
        int max = cls->max_limit;
        e.ip = ip_array + min;
        e.n = MIN(max, ret - min);
        cb->callback(&e, user);
        return 0;
    }
    return -ENOENT;
}

static ssize_t fast_symbol_transform(struct backtrace_class *cls, void *ip, 
    char *buf, size_t maxlen) {
    if (ip == NULL)
        return -EINVAL;
    char **ss = backtrace_symbols(&ip, 1);
    if (ss) {
        size_t len = strlen(*ss);
        size_t cplen = MIN(maxlen - 1, len);
        strncpy(buf, *ss, cplen);
        buf[cplen] = '\0';
        free(ss);
        return cplen;
    }
    return -ENOENT;
}

int backtrace_init(enum bracktrace_type type, struct backtrace_class *cls) {
    memset(cls, 0, sizeof(*cls));
    if (type == UNWIND_BACKTRACE) {
        cls->backtrace = unix_backtrace;
        cls->transform = unix_symbol_transform;
        cls->transform_prepare = unix_transform_prepare;
        cls->transform_post = unix_transform_post;
    } else {
        cls->backtrace = fast_backtrace;
        cls->transform = fast_symbol_transform;
    }
    cls->min_limit = 1;
    cls->max_limit = BACKTRACE_MAX_LIMIT;
    strcpy(cls->separator, "/");
    return 0;
}
