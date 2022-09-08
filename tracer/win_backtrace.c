/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "base/utils.h"
#include "tracer/backtrace.h"

#define SW_IMPL
#include "win/stackwalker.h"

#pragma comment (lib,"imagehlp.lib")

#define BACKTRACE_OPTIONS SW_OPTIONS_SYMBOL //(SW_OPTIONS_SYMBOL|SW_OPTIONS_SOURCEPOS)
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

static void win_fast_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    struct backtrace_entry e;
    void *ip_array[BACKTRACE_MAX_LIMIT];
    size_t ret;

    cb->begin(user);
    ret = CaptureStackBackTrace(cls->min_limit, cls->max_limit, ip_array, NULL);
    if (ret > 0) {
        e.ip = ip_array;
        e.n = ret;
        cb->callback(&e, user);
    }
    cb->end(user);
}

static int win_fast_symbol_entry(struct backtrace_class *cls, 
    void *ip, char *sym, size_t max, void *context) {
    unsigned long buffer[sizeof(SYMBOL_INFO) + 256];
    if (ip == NULL)
        return -EINVAL;
    HANDLE hprocess = *(HANDLE *)context;
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
    symbol->MaxNameLen   = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    SymFromAddr(hprocess, (DWORD64)ip, 0, symbol);
    size_t cplen = MIN(max - 1, symbol->NameLen);
    strncpy(sym, symbol->Name, cplen);
    sym[cplen] = '\0';
    return 0;
}

static void win_begin(struct backtrace_class *cls, void *context, bool to_symbol) {
    if (!to_symbol) {
        HANDLE proc = GetCurrentProcess();
        *(HANDLE *)context = proc;
    } else {
        HANDLE hprocess = *(HANDLE *)context;
        SymInitialize(hprocess, NULL, TRUE);
    }
}

int backtrace_init(enum bracktrace_type type, struct backtrace_class *cls) {
    if (type == UNWIND_BACKTRACE) {
        cls->backtrace = win_backtrace;
        cls->sym_entry = NULL;
        cls->min_limit = 5;
    } else {
        cls->backtrace = win_fast_backtrace;
        cls->sym_entry = win_fast_symbol_entry;
        cls->begin = win_begin;
        cls->min_limit = 1;
        cls->ctx_size = sizeof(HANDLE);
    }
    cls->min_limit = 5;
    cls->max_limit = BACKTRACE_MAX_LIMIT;
    return 0;
}
