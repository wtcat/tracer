/*
 * Copyright 2022 wtcat
 */
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "base/printer.h"
#include "tracer/tracer_core.h"
#include "tracer/mem_tracer.h"

#define PTR_TABLE_SIZE 50
#define TEST_FUN(name) \
    static void name(void)

#define TEST_MALLOC(size) \
    do {\
        assert(ptr_index < PTR_TABLE_SIZE); \
        ptr_table[ptr_index++] = mem_tracer_alloc(size); \
    } while (0)

#define TEST_FREE() \
    do { \
        for (int i = 0; i < ptr_index; i++) {\
            mem_tracer_free(ptr_table[i]); \
            ptr_table[i] = NULL; \
        } \
    } while (0)

static void *ptr_table[40];
static int ptr_index;

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
    void *p1, *p2;

    printf_printer_init(&cout);

    puts("\n\n**************** TEST-1 *****************\n");
    p1 = mem_tracer_alloc(12);
    p2 = mem_tracer_alloc(36);
    mem_tracer_dump(&cout, MEM_SEQUEUE_DUMP);
    mem_tracer_dump(&cout, MEM_SORTED_DUMP);
    mem_tracer_free(p1);
    mem_tracer_free(p2);
    mem_tracer_destory();

    puts("\n\n**************** TEST-2 *****************\n");
    func_5();
    mem_tracer_dump(&cout, MEM_SEQUEUE_DUMP);
    puts("\n\n");
    mem_tracer_dump(&cout, MEM_SORTED_DUMP);

    puts("\n\n**************** TEST-3 *****************\n");
    TEST_FREE();
    mem_tracer_dump(&cout, MEM_SEQUEUE_DUMP);
    mem_tracer_destory();
    return 0;
}