static_library(tracer)

target_sources(tracer
    PRIVATE
    backtrace.c
    tracer_core.c
    mem_tracer.c
)
