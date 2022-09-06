static_library(tracer)

target_sources(tracer
    PRIVATE
    tracer_core.c
    mem_tracer.c
)
