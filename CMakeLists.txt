cmake_minimum_required(VERSION 3.15)

project(gwatch)

#libgit2 options
set(BUILD_SHARED_LIBS OFF)
set(BUILD_CLAR OFF)
set(USE_SSH OFF)
set(USE_HTTPS OFF)
if(MSVC)
    set(STATIC_CRT ON)
endif()

add_subdirectory(lib/libgit2)
add_subdirectory(lib/libuv)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    main.c
)

add_executable(gwatch ${SOURCES})

target_compile_options(gwatch
PRIVATE
    -pedantic
    -pedantic-errors
    -Wall
    -Wextra
    -Werror
    -Wabi=11
    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wdisabled-optimization
    -Wduplicated-cond
    -Wfloat-equal
    -Wformat=2
    -Winit-self
    -Wlogical-op
    -Wmissing-include-dirs
    -Wnull-dereference
    -Wredundant-decls
    -Wshadow
    -Wsign-conversion
    -Wstrict-overflow=5
    -Wswitch-default
    -Wundef
    -Wunused
)

target_include_directories(gwatch
SYSTEM
PRIVATE
    lib/libgit2/include
    lib/libuv/include
)

target_link_libraries(gwatch
    git2
    uv_a
)
