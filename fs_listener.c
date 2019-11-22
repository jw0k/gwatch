#include "fs_listener.h"
#include "fs_oper.h"
#include "args.h"
#include "logs.h"

#include <stdlib.h>
#include <string.h>

uv_loop_t* loop_fs;
uv_timer_t low_pass_timer;
uv_timer_t retry_timer;
void(*cb)() = NULL;

void fs_cb(uv_fs_event_t* handle, const char* filename, int events, int status);
void lp_cb(uv_timer_t* handle);
void start_retry_timer();

void fs_listener_start(uv_loop_t* loop, void(*callback)())
{
    loop_fs = loop;
    cb = callback;

    if (dir_exists(get_repo_path()))
    {
        uv_timer_init(loop_fs, &low_pass_timer);
        fs_listener_start_impl(loop, fs_cb);
    }
    else
    {
        uv_timer_init(loop_fs, &retry_timer);
        start_retry_timer();
    }
}

void fs_listener_stop(uv_fs_event_t* handle)
{
    fs_listener_stop_impl(handle);
}

void lp_cb(uv_timer_t* handle)
{
    (void)handle;
    cb();
    fs_listener_start(loop_fs, cb);
}

void retry_cb(uv_timer_t* handle)
{
    (void)handle;
    if (dir_exists(get_repo_path()))
    {
        cb();
    }
    fs_listener_start(loop_fs, cb);
}

void start_lp_timer()
{
    uv_timer_start(&low_pass_timer, lp_cb, (uint64_t)get_timeout()*1000, 0);
}

void start_retry_timer()
{
    uv_timer_start(&retry_timer, retry_cb, (uint64_t)get_timeout()*1000, 0);
}

void fs_cb(uv_fs_event_t* handle, const char* filename, int events, int status)
{
    (void)status;

    if (events & UV_CHANGE)
        pflog("File changed - %s, starting timer", filename);

    if (events & UV_RENAME)
        pflog("File (re)moved - %s, starting timer", filename);

    fs_listener_stop(handle);
    start_lp_timer();
}
