/*
 * Copyright 2022 wtcat
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base/utils.h"
#include "base/backtrace.h"
#include "base/printer.h"


static inline size_t core_record_ip_size(const struct trace_path *n) {
    return n->max_depth - n->sp;
}

static inline void *core_record_ip(const struct trace_path *n) {
    return n->ip + n->sp;
}

static int trace_path_copy_ip(struct trace_path *node, const void *ip[], size_t n) {
    if (node == NULL || ip == NULL)
        return -EINVAL;
    size_t size = MIN(node->sp, n);
    size_t i = 0, sp = node->sp;
    while (size > 0) {
        node->ip[sp - i - 1] = (void *)ip[i];
        i++;
        size--;
    }
    node->sp = sp - i;
    return 0;
}

static void path_backtrace_begin(void *user) {
    struct backtrace_class *tracer = (struct backtrace_class *)user;
    struct trace_path *node = (struct trace_path *)tracer->context;
    backtrace_begin(tracer, node->context, false);
}

static void path_backtrace_entry(struct backtrace_entry *entry, void *user) {
    struct backtrace_class *tracer = (struct backtrace_class *)user;
    struct trace_path *node = (struct trace_path *)tracer->context;
    trace_path_copy_ip(node, (const void **)entry->ip, entry->n);
}

static void path_backtrace_end(void *user) {
    struct backtrace_class *tracer = (struct backtrace_class *)user;
    struct trace_path *node = (struct trace_path *)tracer->context;
    backtrace_end(tracer, node->context, false);
}

static struct backtrace_callbacks callbacks = {
    .begin = path_backtrace_begin,
    .callback = path_backtrace_entry,
    .end = path_backtrace_end
};

void core_path_init(struct trace_path *path, 
    void *buffer, size_t depth) {
    path->max_depth = depth;
    path->ip = (void **)buffer;
    path->sp = depth;
    path->context = path->ip;
}

void core_extract_path(struct backtrace_class *tracer, struct trace_path *path) {
    tracer->context = path;
    do_backtrace(tracer, &callbacks, tracer);
}

ssize_t core_transform_path(struct backtrace_class *tracer, struct trace_path *node, 
    char *buffer, size_t maxlen, const char *separator) {
    size_t n = core_record_ip_size(node);
    void **ip = core_record_ip(node);
    size_t slen = strlen(separator);
    size_t remain = maxlen - 1;
    size_t offset;
    int splen;
    int ret;

    if (slen >= remain) {
        ret = -EINVAL;
        goto _end;
    }
    memcpy(buffer, separator, slen);
    offset = slen;
    remain -= slen;
    backtrace_begin(tracer, node->context, true);
    for (size_t i = 0; i < n && remain > 0; i++) {
        int ret = backtrace_addr2symbol(tracer, ip[i], buffer+offset, 
            remain, node->context);
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
    backtrace_end(tracer, node->context, true);
    return ret;
}