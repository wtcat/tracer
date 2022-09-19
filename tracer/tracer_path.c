/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <stdlib.h>

#include "base/backtrace.h"
#include "base/ipnode.h"
#include "tracer/tracer_path.h"

struct ip_data {
    struct ip_record ipr;
    void* path[];
};

_Static_assert(PATH_RESERVED_SIZE * sizeof(void *) >= sizeof(struct ip_record), "");

static void tracer_entry(const struct backtrace_entry* entry, void* user) {
    struct ip_data* d = (struct ip_data*)user;
    ip_copy(&d->ipr, entry->ip, entry->n);
}

static struct backtrace_callbacks callbacks = {
    .callback = tracer_entry
};

int tracer_node_init(tracer_pnode_t d, size_t depth) {
    if (d == NULL && depth <= 0)
        return -EINVAL;
    struct ip_data* pd = (struct ip_data*)d;
    pd->ipr.max_depth = depth;
    pd->ipr.sp = depth;
    pd->ipr.ip = pd->path;
    return 0;
}

int tracer_generate_path(void* tracer, tracer_pnode_t d) {
    if (tracer == NULL || d == NULL)
        return -EINVAL;
    return backtrace_extract_path(tracer, &callbacks, d);
}

long tracer_transform_path(void* tracer, tracer_pnode_t d,
    char* buffer, size_t maxlen) {
    if (tracer == NULL || d == NULL || buffer == NULL)
        return -EINVAL;
    struct ip_data* pd = (struct ip_data*)d;
    struct ip_array ips;
    ips.ip = ip_first(&pd->ipr);
    ips.n = ip_size(&pd->ipr);
    return backtrace_transform_path(tracer, &ips, buffer, maxlen);
}

void* tracer_create(const char *separator, int min_limit, int max_limit) {
    struct backtrace_class* cls = malloc(sizeof(*cls));
    if (cls) {
        backtrace_init(FAST_BACKTRACE, cls);
        if (max_limit > min_limit && min_limit >= 0)
            backtrace_set_path_window(cls, min_limit, max_limit);
        if (separator)
            backtrace_set_path_separator(cls, separator);
    }
    return cls;
}

void tracer_destory(void* tracer) {
    free(tracer);
}
