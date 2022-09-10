/*
 * Copyright 2022 wtcat
 */
#include <stdio.h>
#include <stdlib.h>

void __assert_func(
    const char* file,
    int         line,
    const char* func,
    const char* failedexpr
) {
    printf("Assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
        failedexpr,
        file,
        line,
        (func) ? ", function: " : "",
        (func) ? func : ""
    );
    exit(1);
}