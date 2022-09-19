
target_sources(tracer
    PRIVATE
    tracer_core.c
    mem_tracer.c
    tracer_path.c
)

if (WINDOWS)
target_sources(tracer
    PRIVATE
    win_backtrace.c
)
else ()
target_sources(tracer
    PRIVATE
    unix_backtrace.c
)
endif ()

