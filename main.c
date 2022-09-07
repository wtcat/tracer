/*
 * Copyright 2022 wtcat
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "base/printer.h"
#include "base/allocator.h"
#include "tracer/backtrace.h"
#include "tracer/mem_tracer.h"

#define PTR_TABLE_SIZE 50
#define TEST_FUN(name) \
    static void name(void)

#define TEST_MALLOC(size) \
    do {\
        assert(ptr_index < PTR_TABLE_SIZE); \
        ptr_table[ptr_index++] = mem_tracer_alloc(&mtrace_context, size); \
    } while (0)

#define TEST_FREE() \
    do { \
        for (int i = 0; i < ptr_index; i++) {\
            mem_tracer_free(&mtrace_context, ptr_table[i]); \
            ptr_table[i] = NULL; \
        } \
    } while (0)

static MTRACER_DEFINE(mtrace_context);
static void *ptr_table[40];
static int ptr_index;
size_t used_size;

static void *mem_alloc(struct mem_allocator *m, size_t size) {
    (void) m;
    size_t *ptr = malloc(size + sizeof(size));
    if (ptr) {
        used_size += size;
        *ptr = size;
        return ptr + 1;
    }
    return NULL;
}

static void mem_free(struct mem_allocator *m, void *ptr) {
    (void) m;
    size_t *p = (size_t *)ptr;
    p--;
    used_size -= *p;
    free(p);
}

static struct mem_allocator allocator = {
    .allocate = mem_alloc,
    .free = mem_free
};

TEST_FUN(func_1) {
    TEST_MALLOC(16);
    TEST_MALLOC(128);
}

TEST_FUN(func_2) {
    func_1();
    TEST_MALLOC(20);
}

TEST_FUN(func_3) {
    func_2();
    TEST_MALLOC(40);
}

TEST_FUN(func_4) {
    func_3();
    TEST_MALLOC(60);
    TEST_MALLOC(32);
    TEST_MALLOC(24);
}

TEST_FUN(func_5) {
    func_4();
    TEST_MALLOC(80);
}

int main(int argc, char *argv[]) {
    struct printer cout;
    printf_printer_init(&cout);
    mem_tracer_init(&mtrace_context);
    mem_tracer_set_allocator(&mtrace_context, &allocator);

    func_5();
    mem_tracer_dump(&mtrace_context, &cout, MEM_SEQUEUE_DUMP);
    mem_tracer_dump(&mtrace_context, &cout, MEM_SORTED_DUMP);

    printf("**Memory Monitor-1: %" PRIu64 "\n", used_size);
    TEST_FREE();
    mem_tracer_dump(&mtrace_context, &cout, MEM_SEQUEUE_DUMP);
    mem_tracer_deinit(&mtrace_context);
    printf("**Memory Monitor-2: %" PRIu64 "\n", used_size);
    return 0;
}