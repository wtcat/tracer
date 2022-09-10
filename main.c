/*
 * Copyright 2022 wtcat
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "base/printer.h"
#include "base/allocator.h"
#include "base/backtrace.h"
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

static void *mem_alloc(struct mem_allocator *m, size_t size, void *user) {
    (void) m;
    (void) user;
    size_t *ptr = malloc(size + sizeof(size));
    if (ptr) {
        used_size += size;
        *ptr = size;
        return ptr + 1;
    }
    return NULL;
}

static void mem_free(struct mem_allocator *m, void *ptr, void *user) {
    (void) m;
    (void) user;
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
    mem_tracer_init(&mtrace_context, &allocator, 
        MEM_CHECK_OVERFLOW | MEM_CHECK_INVALID);
#if defined(__linux__)
    mem_tracer_set_path_separator(&mtrace_context, "\n\t->");
#else
    mem_tracer_set_path_separator(&mtrace_context, "/");
#endif

    func_5();
    mem_tracer_dump(&mtrace_context, MEM_DUMP_SORTED);
    mem_tracer_dump(&mtrace_context, MEM_DUMP_SEQUENCE);

    printf("**Memory Monitor-1: %" PRIu64 "\n", used_size);
    TEST_FREE();
    mem_tracer_dump(&mtrace_context, MEM_DUMP_SEQUENCE);
    mem_tracer_deinit(&mtrace_context);
    printf("**Memory Monitor-2: %" PRIu64 "\n", used_size);
    return 0;
}