#pragma once

#include <uv.h>

void fs_listener_start(uv_loop_t* loop, void(*callback)());
