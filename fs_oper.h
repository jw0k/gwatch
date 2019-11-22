#pragma once

#include <uv.h>

#include <stdbool.h>

bool dir_exists(const char* path);
void fs_listener_start_impl(uv_loop_t* loop, void(*fs_cb)(
        uv_fs_event_t* handle, const char* filename, int events, int status));
void fs_listener_stop_impl(uv_fs_event_t* handle);
