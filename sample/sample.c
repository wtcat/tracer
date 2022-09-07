/*
 * Copyright 2022 wtcat
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tracer/printer.h"
#include "tracer/mem_tracer.h"
#include "sample.h"

static FILE* log_file;
static struct printer cout;
static MTRACER_DEFINE(mtrace_context);

static void _lvgl_mem_deinit(void) {
    mem_tracer_dump(mtrace_context, &cout, MEM_SORTED_DUMP);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    mem_tracer_deinit(mtrace_context);
}

int _lvgl_mem_init(void) {
    log_file = fopen("dump.txt", "w");
    fprintf_printer_init(&cout, log_file);
    mem_tracer_init(mtrace_context);
    atexit(_lvgl_mem_deinit);
    return 0;
}

void* _lvgl_malloc(size_t size) {
    size_t* p = mem_tracer_alloc(mtrace_context, size + sizeof(size_t));
    if (p) {
        *p = size;
        return p + 1;
    }
    return NULL;
}

void _lvgl_free(void* ptr) {
    size_t* p = (size_t*)ptr;
    mem_tracer_free(mtrace_context, p - 1);
}

void* _lvgl_realloc(void* ptr, size_t size) {
    size_t oldsize;
    void* np;
    if (size == 0)
        return ptr;
    if (ptr == NULL)
        return _lvgl_malloc(size);
    oldsize = ((size_t*)ptr)[-1];
    if (oldsize >= size)
        return ptr;
    np = _lvgl_malloc(size);
    if (np) {
        memcpy(np, ptr, oldsize);
        _lvgl_free(ptr);
    }
    return np;
}
