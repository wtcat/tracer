/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <stdio.h>

#include "base/printer.h"

static int printf_plugin(void *context, const char *format, va_list ap) {
    (void) context;
    return vprintf(format, ap);
}

static int fprintf_plugin(void *context, const char *fmt, va_list ap) {
    return vfprintf(context, fmt, ap);
}

static int sprintf_plugin(void *context, const char *fmt, va_list ap) {
    struct sprintf_context *sctx = (struct sprintf_context *)context;
    if (sctx->buffer == NULL)
        return -EINVAL;
    int remain = sctx->size - sctx->ptr;
    if (remain > 0) {
        int len = vsnprintf(sctx->buffer, remain, fmt, ap);
        sctx->ptr += len;
        return len;
    }
    return 0;
}

void printf_printer_init(struct printer *printer) {
    printer->context = NULL;
    printer->printer = printf_plugin;
}

void fprintf_printer_init(struct printer *printer, void *fp) {
    printer->context = fp;
    printer->printer = fprintf_plugin;
}

void sprintf_printer_init(struct printer *printer, void *sctx) {
    printer->context = sctx;
    printer->printer = sprintf_plugin; 
}

