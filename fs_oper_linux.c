#include "fs_oper.h"
#include "args.h"
#include "logs.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

uv_fs_event_t* fs_event_reqs = NULL;
unsigned int num_fs_events = 0;
unsigned int fs_events_capacity = 0;

bool dir_exists(const char* path)
{
    DIR* dir = NULL;
    dir = opendir(path);
    bool result = (dir != NULL);
    if (dir)
    {
        closedir(dir);
    }
    return result;
}

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

void listen_dirs_recursively(uv_loop_t* loop, void(*fs_cb)(
        uv_fs_event_t* handle, const char* filename, int events, int status),
        const char *name)
{
    DIR* dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return;

    uv_fs_event_t fs_event_r;
    uv_fs_event_init(loop, &fs_event_r);
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
            listen_dirs_recursively(loop, fs_cb, path);
        }
    }

    closedir(dir);
}

void fs_listener_start_impl(uv_loop_t* loop, void(*fs_cb)(
        uv_fs_event_t* handle, const char* filename, int events, int status))
{
    listen_dirs_recursively(loop, fs_cb, get_repo_path());
}

void fs_listener_stop_impl(uv_fs_event_t* handle)
{
    (void)handle;
    unsigned int i;
    for (i = 0; i < num_fs_events; ++i)
        uv_fs_event_stop(fs_event_reqs + i);
    clear_fs_event_reqs();
}
