/*
 * Copyright 2022 wtcat
 */
#include "tracer/backtrace.h"

#if defined(_WIN32)
#define SW_IMPL
#include "win/stackwalker.h"

#define BACKTRACE_OPTIONS (SW_OPTIONS_SYMBOL|SW_OPTIONS_SOURCEPOS)
struct callback_args {
    struct backtrace_callbacks *cb;
    void *arg;
};

static void callstack_begin(void* userptr) {
    struct callback_args *call = (struct callback_args *)userptr;
    call->cb->begin(call->arg);
}

static void callstack_entry(const sw_callstack_entry* entry, void* userptr) {
    struct callback_args *call = (struct callback_args *)userptr;
    struct backtrace_entry e;
    e.symbol = entry->und_name;
    e.pc = 0;
    e.line = entry->line;
    call->cb->callback(&e, call->arg);
}

static void callstack_end(void* userptr) {
    struct callback_args *call = (struct callback_args *)userptr;
    call->cb->end(call->arg);
}

static sw_callbacks callbacks = {
    .callstack_begin = callstack_begin,
    .callstack_entry = callstack_entry,
    .callstack_end = callstack_end
};

static void win_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    static struct callback_args cb_args;
    sw_context* context;

    cb_args.cb = cb;
    cb_args.arg = user;
    context = sw_create_context_capture(BACKTRACE_OPTIONS, &callbacks, &cb_args);
    sw_set_callstack_limits(context, cls->min_limit, cls->max_limit);
    if (!context) {
        puts("ERROR: stackwalk init");
        return;
    }
    sw_show_callstack(context, NULL);
    sw_destroy_context(context);
}

static struct backtrace_class _win_backtrace_class_inst = {
    .backtrace = win_backtrace,
    .context = NULL,
    .min_limit = 0,
    .max_limit = BACKTRACE_MAX_LIMIT
};

#else //!_WIN32
#include <libunwind.h>

static void unix_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    struct backtrace_entry entry;
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip, offp;
    int start, max;
    char name[256];

    start = cls->min_limit;
    max = cls->max_limit;
    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    cb->begin(user);
    while (max > 0 && unw_step(&cursor) > 0) {
        if (start > 0) {
            start--;
            continue;
        }
        max--;
        name[0] = '\0';
        unw_get_proc_name(&cursor, name, sizeof(name), &offp);
        unw_get_reg(&cursor, UNW_REG_IP, &ip);

        /* Fill backtrace information */
        entry.symbol = name;
        entry.pc = ip;
        entry.line = -1;
        cb->callback(&entry, user);
    }
    cb->end(user);
}

static struct backtrace_class _unix_backtrace_class_inst = {
    .backtrace = unix_backtrace,
    .context = NULL,
    .min_limit = 0,
    .max_limit = BACKTRACE_MAX_LIMIT
};
#endif //_WIN32

struct backtrace_class *backtrace_get_instance(void) {
#if defined(_WIN32)
    return &_win_backtrace_class_inst;
#else
    return &_unix_backtrace_class_inst;
#endif
}
