/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <string.h>

#include "base/utils.h"
#include "base/backtrace.h"

#include <windows.h>
// #define SW_IMPL
// #include "win/stackwalker.h"

// #pragma comment (lib,"imagehlp.lib")

struct win_context {
    HANDLE handle;
};

static int win_backtrace(struct backtrace_class *cls, struct backtrace_callbacks *cb, 
    void *user) {
    void *ip_array[BACKTRACE_MAX_LIMIT];
    size_t ret;
    ret = CaptureStackBackTrace(cls->min_limit, cls->max_limit, ip_array, NULL);
    if (ret > 0) {
        struct backtrace_entry e;
        e.ip = ip_array;
        e.n = ret;
        cb->callback(&e, user);
        return 0;
    }
    return -ENOENT;
}

static ssize_t win_symbol_transform(struct backtrace_class *cls, void *ip, 
    char *buf, size_t maxlen) {
    unsigned long buffer[sizeof(SYMBOL_INFO) + 256];
    if (ip == NULL)
        return -EINVAL;
    struct win_context *ctx = (struct backtrace_class *)cls->context;
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
    symbol->MaxNameLen   = 255;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    SymFromAddr(ctx->handle, (DWORD64)ip, 0, symbol);
    size_t cplen = MIN(maxlen - 1, symbol->NameLen);
    strncpy(buf, symbol->Name, cplen);
    buf[cplen] = '\0';
    return symbol->NameLen;
}

static int win_transform_prepare(struct backtrace_class *cls) {
    struct win_context *ctx = (struct backtrace_class *)cls->context;
    SymInitialize(ctx->handle, NULL, TRUE);
    return 0;
}

int backtrace_init(enum bracktrace_type type, struct backtrace_class *cls) {
    (void) type;
    static struct win_context win;
    cls->backtrace = win_backtrace;
    cls->transform = win_symbol_transform;
    cls->transform_prepare = win_transform_prepare;
    cls->min_limit = 1; 
    cls->min_limit = 5;
    cls->max_limit = BACKTRACE_MAX_LIMIT;
    win.handle = GetCurrentProcess();
    cls->context = &win;
    strcpy(cls->separator, "/");
    return 0;
}
