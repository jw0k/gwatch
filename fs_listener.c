#include "fs_listener.h"
#include "args.h"
#include "logs.h"

#include <stdlib.h>
#include <string.h>

uv_loop_t* loop_fs;
uv_timer_t low_pass_timer;
uv_timer_t retry_timer;
void(*cb)() = NULL;

#ifdef WIN32
uv_fs_event_t fs_event_req;
#else
uv_fs_event_t* fs_event_reqs = NULL;
unsigned int num_fs_events = 0;
unsigned int fs_events_capacity = 0;
#endif

void fs_cb(uv_fs_event_t* handle, const char* filename, int events, int status);
void lp_cb(uv_timer_t* handle);
void start_retry_timer();

#ifndef WIN32
void add_fs_event_req(uv_fs_event_t* fs_event)
{
    if (fs_event_reqs == NULL)
    {
        fs_events_capacity = 16;
        fs_event_reqs = (uv_fs_event_t*)malloc(fs_events_capacity
                * sizeof(uv_fs_event_t));
    }
    if (num_fs_events >= fs_events_capacity)
    {
        fs_events_capacity *= 2;
        void* new_buffer = realloc(fs_event_reqs, fs_events_capacity
                * sizeof(uv_fs_event_t));
        if (new_buffer == NULL)
        {
            plog("Cannot realloc fs events buffer");
            exit(1);
        }
        fs_event_reqs = (uv_fs_event_t*)new_buffer;
    }
    memcpy(fs_event_reqs + num_fs_events, fs_event, sizeof(uv_fs_event_t));
    ++num_fs_events;
}

void clear_fs_event_reqs()
{
    if (fs_event_reqs)
    {
        free(fs_event_reqs);
        fs_event_reqs = NULL;
        num_fs_events = 0;
    }
}
#endif

#ifndef WIN32
#include <sys/types.h>
#include <dirent.h>
void listen_dirs_recursively(const char *name)
{
    DIR* dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;

    uv_fs_event_t fs_event_r;
    uv_fs_event_init(loop_fs, &fs_event_r);
    add_fs_event_req(&fs_event_r);
    uv_fs_event_start(fs_event_reqs + num_fs_events - 1, fs_cb, name, 0);

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_DIR)
        {
            char path[2048];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            listen_dirs_recursively(path);
        }
    }

    closedir(dir);
}
#endif

bool dir_exists(const char* path)
{
#ifdef WIN32
    DWORD dwAttrib = GetFileAttributes(path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    DIR* dir = NULL;
    dir = opendir(path);
    bool result = (dir != NULL);
    if (dir)
    {
        closedir(dir);
    }
    return result;
#endif
}

void fs_listener_start(uv_loop_t* loop, void(*callback)())
{
    loop_fs = loop;
    cb = callback;

    if (dir_exists(get_repo_path()))
    {
        uv_timer_init(loop_fs, &low_pass_timer);
#ifdef WIN32
        uv_fs_event_init(loop, &fs_event_req);
        uv_fs_event_start(&fs_event_req, fs_cb, get_repo_path()(),
                UV_FS_EVENT_RECURSIVE);
#else
        listen_dirs_recursively(get_repo_path());
#endif
    }
    else
    {
        uv_timer_init(loop_fs, &retry_timer);
        start_retry_timer();
    }
}

void stop_fs_listener(uv_fs_event_t* handle)
{
#ifdef WIN32
    uv_fs_event_stop(handle);
#else
    (void)handle;
    unsigned int i;
    for (i = 0; i < num_fs_events; ++i)
        uv_fs_event_stop(fs_event_reqs + i);
    clear_fs_event_reqs();
#endif
}

void lp_cb(uv_timer_t* handle)
{
    (void)handle;
    cb();
}

void retry_cb(uv_timer_t* handle)
{
    (void)handle;
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

    stop_fs_listener(handle);
    start_lp_timer();
}
