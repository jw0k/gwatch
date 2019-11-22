cmake_minimum_required(VERSION 3.15)

project(gwatch)

#libgit2 options
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_CLAR OFF CACHE BOOL "" FORCE)
set(USE_SSH OFF CACHE BOOL "" FORCE)
set(USE_HTTPS OFF CACHE BOOL "" FORCE)
if(MSVC)
    set(STATIC_CRT ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(lib/libgit2)
add_subdirectory(lib/libuv)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    main.c
    logs.h
    logs.c
    args.c
    args.h
    fs_listener.h
    fs_listener.c
    git.c
    git.h
    fs_oper.h
)

if(MSVC)
    list(APPEND SOURCES
        fs_oper_win.c
    )
else()
    list(APPEND SOURCES
        fs_oper_linux.c
    )
endif()

add_executable(gwatch ${SOURCES})

if(MSVC)
	if(STATIC_CRT)
		set_target_properties(gwatch
		PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
		)
	endif()
endif()

set_target_properties(gwatch
PROPERTIES
    C_STANDARD 11
)

if (UNIX)
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
else()
	target_compile_options(gwatch
	PRIVATE
		/W3
	)
endif()

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
