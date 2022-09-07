cmake_minimum_required(VERSION 3.8)
# include(cmake/clang.cmake)
project(app)
enable_language(C CXX)

#Extensions
include(cmake/extensions.cmake)

if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
endif(POLICY CMP0076)

set(TARGET_NAME ${PROJECT_NAME})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

# Select operation system
message(STATUS "OS Platform: ${CMAKE_SYSTEM_NAME}")
if (CMAKE_HOST_WIN32)
    enable_language(RC)
    set(WINDOWS TRUE)
    set(TARGET_NAME "${PROJECT_NAME}.exe")
    add_compile_options(
        /wd4996
    )
elseif (CMAKE_HOST_APPLE)
    set(MACOS TRUE)
    set(TARGET_NAME "${PROJECT_NAME}.elf")
elseif (CMAKE_HOST_UNIX)
    set(LINUX TRUE)
    set(TARGET_NAME "${PROJECT_NAME}.elf")
else ()
    set(TARGET_NAME "${PROJECT_NAME}")
    message(FATAL_ERROR "Unknown operation system")
endif ()


# Global compile options
include_directories(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/include
)
add_compile_options(
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>>:-rdynamic>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>>:-Wall>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>>:-g>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<C_COMPILER_ID:GNU>>:-O0>
)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)

# Generate executable file
add_executable(${TARGET_NAME} 
    main.c
)

static_library(tracer)
add_subdirectory(base)
add_subdirectory(tracer)

# Link target
collect_link_libraries(LIBS ${TARGET_NAME})
if (NOT WINDOWS)
target_link_libraries(${TARGET_NAME} 
    -Wl,--start-group
    ${LIBS}
    -Wl,--end-group
    unwind
    unwind-x86_64
    pthread
)
else ()
target_link_libraries(${TARGET_NAME} 
    ${LIBS}
)
endif ()

# Install
set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR})

install(FILES 
    base/allocator.h
    base/printer.h
    tracer/mem_tracer.h
    DESTINATION _install/include/tracer)

install(TARGETS tracer
    # EXPORT MyMathTargets
    LIBRARY DESTINATION _install
    ARCHIVE DESTINATION _install
    RUNTIME DESTINATION _install
)

# User target
if (NOT WINDOWS)
add_custom_target(lldb
    COMMAND
    lldb ${TARGET_NAME}
)
add_custom_target(gdb
    COMMAND
    gdb ${TARGET_NAME}
)
endif ()
