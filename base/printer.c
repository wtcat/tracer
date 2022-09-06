/*
 * Copyright 2022 wtcat
 */
#include <stdio.h>

#include "base/printer.h"

static int printf_plugin(void *context, const char *format, va_list ap) {
    (void) context;
    return vprintf(format, ap);
}

static int fprintf_plugin(void *context, const char *fmt, va_list ap) {
    return vfprintf(context, fmt, ap);
}

void printf_printer_init(struct printer *printer) {
    printer->context = NULL;
    printer->printer = printf_plugin;
}

void fprintf_printer_init(struct printer *printer, void *fp) {
    printer->context = fp;
    printer->printer = fprintf_plugin;
}
