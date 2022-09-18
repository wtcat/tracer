/*
 * Copyright 2022 wtcat
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base/assert.h"
#include "base/utils.h"
#include "base/backtrace.h"

static inline void user_backtrace_begin(struct backtrace_callbacks *cb, 
    struct backtrace_class *cls, void *user) {
    if (cb->begin)
        cb->begin(cls, user);
}

static inline void user_backtrace_end(struct backtrace_callbacks *cb, 
    struct backtrace_class *cls, void *user, int err) {
    if (cb->end)
        cb->end(cls, user, err);
}

static inline ssize_t backtrace_addr2symbol(struct backtrace_class *cls, void *ip, 
    char *buf, size_t maxlen) {
    return cls->transform(cls, ip, buf, maxlen);
}

static inline int bactrace_symbol_init(struct backtrace_class *cls) {
    if (cls->transform_prepare)
        return cls->transform_prepare(cls);
    return 0; 
}

static inline void bactrace_symbol_deinit(struct backtrace_class *cls) {
    if (cls->transform_post)
        cls->transform_post(cls);
}

int backtrace_set_path_window(struct backtrace_class *cls, 
    int min_limit, int max_limit) {
    if (cls == NULL)
        return -EINVAL;
    if (!max_limit)
        max_limit = BACKTRACE_MAX_LIMIT;
    cls->min_limit = min_limit;
    cls->max_limit = max_limit;
    return 0;
}

int backtrace_set_path_separator(struct backtrace_class *cls, const char *separator) {
    ASSERT_TRUE(cls != NULL);
    if (separator == NULL)
        return -EINVAL;
    memset(cls->separator, 0, BACKTRACE_SEPARATOR_SIZE);
    strncpy(cls->separator, separator, BACKTRACE_SEPARATOR_SIZE-1);
    return 0;
}

int backtrace_extract_path(struct backtrace_class *cls, 
    struct backtrace_callbacks *cb, void *user) {
    ASSERT_TRUE(cls != NULL);
    ASSERT_TRUE(cb != NULL);
    if (cb->callback == NULL)
        return -EINVAL;
    user_backtrace_begin(cb, cls, user);
    int ret = do_backtrace(cls, cb, user);
    user_backtrace_end(cb, cls, user, ret);
    return ret;
}

ssize_t backtrace_transform_path(struct backtrace_class *cls, struct ip_array *ips, 
    char *buffer, size_t maxlen) {
    ASSERT_TRUE(cls != NULL);
    if (ips == NULL || buffer == NULL)
        return -EINVAL;
    const char *separator = cls->separator;
    size_t n = ips->n;
    void **ip = ips->ip;
    size_t slen = strlen(separator);
    size_t remain = maxlen - 1;
    size_t offset, i;
    int splen;
    int ret;

    if (slen >= remain) {
        ret = -EINVAL;
        goto _end;
    }
    ret = bactrace_symbol_init(cls);
    if (ret)
        goto _end;
    memcpy(buffer, separator, slen);
    offset = slen;
    remain -= slen;
    for (i = 0; i < n && remain > 0; i++) {
        int ret = backtrace_addr2symbol(cls, ip[i], buffer+offset, 
            remain);
        if (ret > 0) {
            offset += ret;
            if (offset + slen >= maxlen) 
                break;

            memcpy(buffer+offset, separator, slen);
            offset += slen;
            remain = maxlen - offset;
            continue;
        }
        splen = snprintf(buffer+offset, remain, "%p", ip[i]);
        offset += splen;
        remain -= splen;
    }
    buffer[offset] = '\0';
    ret = offset;
_end:
    bactrace_symbol_deinit(cls);
    return ret;
}
