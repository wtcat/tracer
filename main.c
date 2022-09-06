/*
 *
 */
#include <stdbool.h>
#include <stdio.h>

#include "base/printer.h"
#include "tracer/tracer_core.h"
#include "tracer/mem_tracer.h"


int main(int argc, char *argv[]) {
    struct printer cout;
    void *p1, *p2;

    printf_printer_init(&cout);
    p1 = mem_tracer_alloc(12);
    p2 = mem_tracer_alloc(36);
    mem_tracer_dump(&cout, MEM_SEQUEUE_DUMP);
    mem_tracer_dump(&cout, MEM_SORTED_DUMP);
    mem_tracer_free(p1);
    mem_tracer_free(p2);
    mem_tracer_destory();
    return 0;
}