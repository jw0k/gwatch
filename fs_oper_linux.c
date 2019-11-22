#include "fs_oper.h"
#include "args.h"
#include "lib/libuv/include/uv.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct fs_event_req_node
{
    uv_fs_event_t fs_event_req;
    struct fs_event_req_node* next;
};
typedef struct fs_event_req_node fs_event_req_node;
fs_event_req_node* fs_event_req_head = NULL;
fs_event_req_node* fs_event_req_tail = NULL;
bool free_on_next_add = false;

void free_list();

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

fs_event_req_node* add_to_list()
{
    if (free_on_next_add)
    {
        free_list();
        free_on_next_add = false;
    }

    if (fs_event_req_head == NULL)
    {
        fs_event_req_head = fs_event_req_tail = malloc(sizeof(fs_event_req_node));
        fs_event_req_head->next = NULL;
    }
    else
    {
        fs_event_req_tail->next = malloc(sizeof(fs_event_req_node));
        fs_event_req_tail = fs_event_req_tail->next;
        fs_event_req_tail->next = NULL;
    }

    return fs_event_req_tail;
}

void free_list()
{
    fs_event_req_node* it = fs_event_req_head;
    while (it)
    {
        fs_event_req_node* temp = it;
        it = it->next;
        free(temp);
    }
    fs_event_req_head = fs_event_req_tail = NULL;
}

void listen_dirs_recursively(uv_loop_t* loop, void(*fs_cb)(
        uv_fs_event_t* handle, const char* filename, int events, int status),
        const char *name)
{
    DIR* dir;
    struct dirent* entry;

    if (!(dir = opendir(name)))
        return;

    fs_event_req_node* node = add_to_list();
    uv_fs_event_init(loop, &node->fs_event_req);
    uv_fs_event_start(&node->fs_event_req, fs_cb, name, 0);

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

void close_cb(uv_handle_t* handle)
{
    (void)handle;
    free_on_next_add = true;
}

void fs_listener_stop_impl(uv_fs_event_t* handle)
{
    (void)handle;

    fs_event_req_node* it = fs_event_req_head;
    while (it)
    {
        uv_fs_event_stop(&it->fs_event_req);
        uv_close((uv_handle_t*)(&it->fs_event_req), close_cb);
        it = it->next;
    }
}
