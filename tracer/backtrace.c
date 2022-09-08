/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "base/utils.h"
#include "tracer/backtrace.h"


#if defined(_WIN32)
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
        SymInitialize(hprocess, NULL, TRUE );
    }
}

#else //!_WIN32
#include <string.h>
#include <execinfo.h>
#include <libunwind.h>

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

#endif //_WIN32

void backtrace_init(enum bracktrace_type type, struct backtrace_class *cls) {
    memset(cls, 0, sizeof(*cls));
#if defined(_WIN32)
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
#else
    if (type == UNWIND_BACKTRACE) {
        cls->backtrace = unix_backtrace;
        cls->sym_entry = NULL;
    } else {
        cls->backtrace = fast_backtrace;
        cls->sym_entry = fast_symbol_entry;
    }
    cls->min_limit = 1;
    cls->max_limit = BACKTRACE_MAX_LIMIT;
#endif
}
